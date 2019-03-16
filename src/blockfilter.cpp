// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include <sstream>

#include <blockfilter.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <streams.h>
#include <validation/validation.h>
#include "blockstore.h"
#include "checkpoints.h"

/// SerType used to serialize parameters in GCS filter encoding.
static constexpr int GCS_SER_TYPE = SER_NETWORK;

/// Protocol version used to serialize parameters in GCS filter encoding.
static constexpr int GCS_SER_VERSION = 0;

static const std::map<BlockFilterType, std::string> g_filter_types = {
    {BASIC, "basic"},
};

template <typename OStream>
static void GolombRiceEncode(BitStreamWriter<OStream>& bitwriter, uint8_t P, uint64_t x)
{
    // Write quotient as unary-encoded: q 1's followed by one 0.
    uint64_t q = x >> P;
    while (q > 0) {
        int nbits = q <= 64 ? static_cast<int>(q) : 64;
        bitwriter.Write(~0ULL, nbits);
        q -= nbits;
    }
    bitwriter.Write(0, 1);

    // Write the remainder in P bits. Since the remainder is just the bottom
    // P bits of x, there is no need to mask first.
    bitwriter.Write(x, P);
}

template <typename IStream>
static uint64_t GolombRiceDecode(BitStreamReader<IStream>& bitreader, uint8_t P)
{
    // Read unary-encoded quotient: q 1's followed by one 0.
    uint64_t q = 0;
    while (bitreader.Read(1) == 1) {
        ++q;
    }

    uint64_t r = bitreader.Read(P);

    return (q << P) + r;
}

// Map a value x that is uniformly distributed in the range [0, 2^64) to a
// value uniformly distributed in [0, n) by returning the upper 64 bits of
// x * n.
//
// See: https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
static uint64_t MapIntoRange(uint64_t x, uint64_t n)
{
#ifdef __SIZEOF_INT128__
    return (static_cast<unsigned __int128>(x) * static_cast<unsigned __int128>(n)) >> 64;
#else
    // To perform the calculation on 64-bit numbers without losing the
    // result to overflow, split the numbers into the most significant and
    // least significant 32 bits and perform multiplication piece-wise.
    //
    // See: https://stackoverflow.com/a/26855440
    uint64_t x_hi = x >> 32;
    uint64_t x_lo = x & 0xFFFFFFFF;
    uint64_t n_hi = n >> 32;
    uint64_t n_lo = n & 0xFFFFFFFF;

    uint64_t ac = x_hi * n_hi;
    uint64_t ad = x_hi * n_lo;
    uint64_t bc = x_lo * n_hi;
    uint64_t bd = x_lo * n_lo;

    uint64_t mid34 = (bd >> 32) + (bc & 0xFFFFFFFF) + (ad & 0xFFFFFFFF);
    uint64_t upper64 = ac + (bc >> 32) + (ad >> 32) + (mid34 >> 32);
    return upper64;
#endif
}

uint64_t GCSFilter::HashToRange(const Element& element) const
{
    uint64_t hash = CSipHasher(m_params.m_siphash_k0, m_params.m_siphash_k1)
        .Write(element.data(), element.size())
        .Finalize();
    return MapIntoRange(hash, m_F);
}

std::vector<uint64_t> GCSFilter::BuildHashedSet(const ElementSet& elements) const
{
    std::vector<uint64_t> hashed_elements;
    hashed_elements.reserve(elements.size());
    for (const Element& element : elements) {
        hashed_elements.push_back(HashToRange(element));
    }
    std::sort(hashed_elements.begin(), hashed_elements.end());
    return hashed_elements;
}

GCSFilter::GCSFilter(const Params& params)
    : m_params(params), m_N(0), m_F(0), m_encoded{0}
{}

GCSFilter::GCSFilter(const Params& params, std::vector<unsigned char> encoded_filter)
    : m_params(params), m_encoded(std::move(encoded_filter))
{
    VectorReader stream(GCS_SER_TYPE, GCS_SER_VERSION, m_encoded, 0);

    uint64_t N = ReadCompactSize(stream);
    m_N = static_cast<uint32_t>(N);
    if (m_N != N) {
        throw std::ios_base::failure("N must be <2^32");
    }
    m_F = static_cast<uint64_t>(m_N) * static_cast<uint64_t>(m_params.m_M);

    // Verify that the encoded filter contains exactly N elements. If it has too much or too little
    // data, a std::ios_base::failure exception will be raised.
    BitStreamReader<VectorReader> bitreader(stream);
    for (uint64_t i = 0; i < m_N; ++i) {
        GolombRiceDecode(bitreader, m_params.m_P);
    }
    if (!stream.empty()) {
        throw std::ios_base::failure("encoded_filter contains excess data");
    }
}

GCSFilter::GCSFilter(const Params& params, const ElementSet& elements)
    : m_params(params)
{
    size_t N = elements.size();
    m_N = static_cast<uint32_t>(N);
    if (m_N != N) {
        throw std::invalid_argument("N must be <2^32");
    }
    m_F = static_cast<uint64_t>(m_N) * static_cast<uint64_t>(m_params.m_M);

    CVectorWriter stream(GCS_SER_TYPE, GCS_SER_VERSION, m_encoded, 0);

    WriteCompactSize(stream, m_N);

    if (elements.empty()) {
        return;
    }

    BitStreamWriter<CVectorWriter> bitwriter(stream);

    uint64_t last_value = 0;
    for (uint64_t value : BuildHashedSet(elements)) {
        uint64_t delta = value - last_value;
        GolombRiceEncode(bitwriter, m_params.m_P, delta);
        last_value = value;
    }

    bitwriter.Flush();
}

bool GCSFilter::MatchInternal(const uint64_t* element_hashes, size_t size) const
{
    VectorReader stream(GCS_SER_TYPE, GCS_SER_VERSION, m_encoded, 0);

    // Seek forward by size of N
    uint64_t N = ReadCompactSize(stream);
    assert(N == m_N);

    BitStreamReader<VectorReader> bitreader(stream);

    uint64_t value = 0;
    size_t hashes_index = 0;
    for (uint32_t i = 0; i < m_N; ++i) {
        uint64_t delta = GolombRiceDecode(bitreader, m_params.m_P);
        value += delta;

        while (true) {
            if (hashes_index == size) {
                return false;
            } else if (element_hashes[hashes_index] == value) {
                return true;
            } else if (element_hashes[hashes_index] > value) {
                break;
            }

            hashes_index++;
        }
    }

    return false;
}

bool GCSFilter::Match(const Element& element) const
{
    uint64_t query = HashToRange(element);
    return MatchInternal(&query, 1);
}

bool GCSFilter::MatchAny(const ElementSet& elements) const
{
    const std::vector<uint64_t> queries = BuildHashedSet(elements);
    return MatchInternal(queries.data(), queries.size());
}

const std::string& BlockFilterTypeName(BlockFilterType filter_type)
{
    static std::string unknown_retval = "";
    auto it = g_filter_types.find(filter_type);
    return it != g_filter_types.end() ? it->second : unknown_retval;
}

bool BlockFilterTypeByName(const std::string& name, BlockFilterType& filter_type) {
    for (auto entry : g_filter_types) {
        if (entry.second == name) {
            filter_type = entry.first;
            return true;
        }
    }
    return false;
}

const std::vector<BlockFilterType>& AllBlockFilterTypes()
{
    static std::vector<BlockFilterType> retval{
        BlockFilterType::BASIC,
    };
    return retval;
}

std::string ListBlockFilterTypes()
{
    std::stringstream ret;
    bool first = true;
    for (auto filter_type : AllBlockFilterTypes()) {
        if (!first) ret << ", ";
        ret << BlockFilterTypeName(filter_type);
        first = false;
    }
    return ret.str();
}

static void BasicFilterElements(GCSFilter::ElementSet& elements, const CBlock& block, const CBlockUndo& block_undo)
{
    for (const CTransactionRef& tx : block.vtx) {
        for (const CTxOut& txout : tx->vout) {
            const CScript& script = txout.output.scriptPubKey;
            if (script.empty() || script[0] == OP_RETURN) continue;
            elements.emplace(script.begin(), script.end());
        }
    }

    for (const CTxUndo& tx_undo : block_undo.vtxundo) {
        for (const Coin& prevout : tx_undo.vprevout) {
            const CScript& script = prevout.out.output.scriptPubKey;
            if (script.empty()) continue;
            elements.emplace(script.begin(), script.end());
        }
    }

}

static GCSFilter::ElementSet BasicFilterElements(const CBlock& block, const CBlockUndo& block_undo)
{
    GCSFilter::ElementSet elements;
    BasicFilterElements(elements, block, block_undo);
    return elements;
}

BlockFilter::BlockFilter(BlockFilterType filter_type, const uint256& block_hash,
                         std::vector<unsigned char> filter)
    : m_filter_type(filter_type), m_block_hash(block_hash)
{
    GCSFilter::Params params;
    if (!BuildParams(params)) {
        throw std::invalid_argument("unknown filter_type");
    }
    m_filter = GCSFilter(params, std::move(filter));
}

BlockFilter::BlockFilter(BlockFilterType filter_type, const CBlock& block, const CBlockUndo& block_undo)
    : m_filter_type(filter_type), m_block_hash(block.GetHashPoW2())
{
    GCSFilter::Params params;
    if (!BuildParams(params)) {
        throw std::invalid_argument("unknown filter_type");
    }
    m_filter = GCSFilter(params, BasicFilterElements(block, block_undo));
}

bool BlockFilter::BuildParams(GCSFilter::Params& params) const
{
    switch (m_filter_type) {
    case BlockFilterType::BASIC:
        params.m_siphash_k0 = m_block_hash.GetUint64(0);
        params.m_siphash_k1 = m_block_hash.GetUint64(1);
        params.m_P = BASIC_FILTER_P;
        params.m_M = BASIC_FILTER_M;
        return true;
    }

    return false;
}

uint256 BlockFilter::GetHash() const
{
    const std::vector<unsigned char>& data = GetEncodedFilter();

    uint256 result;
    CHash256().Write(data.data(), data.size()).Finalize(result.begin());
    return result;
}

uint256 BlockFilter::ComputeHeader(const uint256& prev_header) const
{
    const uint256& filter_hash = GetHash();

    uint256 result;
    CHash256()
        .Write(filter_hash.begin(), filter_hash.size())
        .Write(prev_header.begin(), prev_header.size())
        .Finalize(result.begin());
    return result;
}

RangedCPBlockFilter::RangedCPBlockFilter(const CBlockIndex* startRange, const CBlockIndex* endRange)
: BlockFilter()
{
    GCSFilter::Params params;
    if (!BuildParams(params))
    {
        throw std::invalid_argument("unknown filter_type");
    }
    GCSFilter::ElementSet elements;
    const CBlockIndex* pIndex = endRange;
    while (true)
    {
        CBlock block;
        if (!ReadBlockFromDisk(block, pIndex, Params()))
        {
            throw std::invalid_argument("BlockFilter: error reading block from disk");
        }
        CBlockUndo blockUndo;
        if (pIndex->pprev)
        {
            CDiskBlockPos pos = pIndex->GetUndoPos();
            if (pos.IsNull())
            {
                throw std::invalid_argument("BlockFilter: error reading undo pos from disk");
            }

            if (!blockStore.UndoReadFromDisk(blockUndo, pos, pIndex->pprev->GetBlockHashPoW2()))
            {
                throw std::invalid_argument("BlockFilter: error reading undo data from disk");
            }
        }
        BasicFilterElements(elements, block, blockUndo);
        if (pIndex == startRange)
            break;
        pIndex=pIndex->pprev;
    }
    m_filter = GCSFilter(params, elements);
}

RangedCPBlockFilter::RangedCPBlockFilter(std::vector<unsigned char> filter)
{
    GCSFilter::Params params;
    if (!BuildParams(params)) {
        throw std::invalid_argument("unknown filter_type");
    }
    m_filter = GCSFilter(params, std::move(filter));
}

bool RangedCPBlockFilter::BuildParams(GCSFilter::Params& params) const
{
    params.m_siphash_k0 = 0;
    params.m_siphash_k1 = 0;
    // Filters have a false positive rate of '1/M' while M needs to be roughly '2^P' for optimal space usage, with some possible variance.
    // Ultimately both P and M, must be altered
    // For our purposes a "relatively high" false positive rate (e.g. about 1%) is tolerable
    // Some non-comprehensive testing of various sizes leaves us with the following rough results to work with
    // Tests done using a test vector of 20 random addresses per run and averaging over several runs
    //gaps: 1000 blocks
    //P=8  M=256    2965052 bytes      false positive rate = (69/908) ranges
    //P=10 M=1034   3585205 bytes      false positive rate = (15/908) ranges
    //P=10 M=2048   3878673 bytes      false positive rate = (9/908) ranges
    //P=6 M=4096    21772036 bytes     false positive rate = (7/908) ranges
    //P=12 M=4096   4199978 bytes      false positive rate = (4/908) ranges
    //gaps: 100 blocks
    //P=12 M=4096   6280492 bytes
    //P=12 M=5700   6449605 bytes
    //We have settled on the current settings for now (using gaps of 100 blocks), however these can easily be changed for future releases
    //P=12 M=4096   FP = (20 / 4603) = 0,0043%
    //When/if we decide on more appropriate settings.
    params.m_P = 12;
    params.m_M = 4096;
    return true;
}


//fixme: (2.1) We can potentially further improve this by indexing only the actual key hashes and not the entire script
//This should be slightly smaller and faster
void getBlockFilterBirthAndRanges(uint64_t nHardBirthDate, uint64_t& nSoftBirthDate, const GCSFilter::ElementSet& walletAddresses, std::vector<std::tuple<uint64_t, uint64_t>>& blockFilterRanges)
{
    std::string dataFilePath = GetArg("-spvstaticfilterfile", "");
    if (dataFilePath.empty())
    {
        LogPrintf("Running without a static filtercp file\n");
        nSoftBirthDate = nHardBirthDate;
        return;
    }
    else
    {
        std::ifstream dataFile(dataFilePath);
        if (!dataFile.good())
        {
            LogPrintf("Failed to read static filtercp file\n");
            nSoftBirthDate = nHardBirthDate;
            return;
        }
        LogPrintf("Loading static filtercp file\n");
        uint64_t nStaticFilterOffset = GetArg("-spvstaticfilterfileoffset", (uint64_t)0);
        uint64_t nStaticFilterLength = GetArg("-spvstaticfilterfilelength", std::numeric_limits<uint64_t>::max());

        uint64_t nStartIndex = 250000;//Earliest possible recovery phrase (before this we didn't use phrases)
        uint64_t nInterval1 = 500;
        uint64_t nInterval2 = 100;
        uint64_t nCrossOver = 500000;
        uint32_t nRanges=0;
        dataFile.seekg(nStaticFilterOffset);
        while (((uint64_t)dataFile.tellg() - nStaticFilterOffset < nStaticFilterLength) && (dataFile.peek() != EOF))
        {
            int nInterval = nInterval1;
            if (nStartIndex >= nCrossOver)
                nInterval = nInterval2;
            size_t nDataSize = ReadCompactSize(dataFile);
            std::vector<unsigned char> data;
            data.resize(nDataSize);
            dataFile.read((char*)&data[0], nDataSize);
            RangedCPBlockFilter rangeFilter(data);
            ++nRanges;
            if (rangeFilter.GetFilter().MatchAny(walletAddresses))
            {
                // Move the birth date forwards as far as possible from the 'hard' birthdate while still including all possible matches
                if (nStartIndex > nHardBirthDate && nStartIndex < nSoftBirthDate)
                {
                    nSoftBirthDate = nStartIndex;
                }
                blockFilterRanges.push_back(std::tuple(nStartIndex, nStartIndex+nInterval));
            }
            nStartIndex+=nInterval;
        }
        LogPrintf("Hard birth block=%d; Soft birth block=%d; Last checkpoint=%d; Addresses=%d; Ranges=%d/%d\n", nHardBirthDate, nSoftBirthDate, Checkpoints::LastCheckPointHeight(), walletAddresses.size(), blockFilterRanges.size(), nRanges);
    }
}
