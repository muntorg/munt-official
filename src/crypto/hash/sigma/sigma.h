// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING

#ifndef CRYPTO_HASH_SIGMA_HASH_H
#define CRYPTO_HASH_SIGMA_HASH_H

#include <stdint.h>
#include <primitives/block.h>

#include <crypto/hash/sigma/argon_echo/argon_echo.h>
#include <crypto/hash/sigma/echo256/sphlib/sph_echo.h>
#include <crypto/hash/sigma/echo256/echo256_opt.h>
#include <crypto/hash/sigma/shavite3_256/shavite3_256_opt.h>
#include <crypto/hash/sigma/shavite3_256/ref/shavite3_ref.h>

//  SIGMA hash
// *S*emi
// *I*terated               - A key part of the algorithm is that parts of the hash can be iterated over cheaply.
// *G*lobal
// *M*emory                 - A large global memory buffer is also a key part of the algorithm
// *A*rgon                  - Argon is used extensively throughout the algorithm
//
// SIGMA attempts to be a 'CPU friendly' PoW algorithm that counteracts the advantages that ASICs and GPU miners would normally have:
// * The primary mechanism through which this is achieved is by requiring large quantities of memory in an asymmetrical way. i.e. for mining memory requirements are large, but verification can be done cheaply.
// * In addition we attempt to rely on computational features where CPUs are already highly optimised (like AES-ni)
// * We build on top of various existing hash functions and simply combine them.
//
// Operation:
// * A large (4gb/8gb/12gb etc.) global arena (G) is populated by performing multiple argon2d (modified to fill memory) hashes of the block (with the nonce zeroed out).
// * The first half of the nonce is iterated X times and for each iteration an argon2d hash (S) is performed of it.
// * For each S the second half of the nonce is then iterated Y times and for each iteration N random indices are generated with a PRNG seeded by S.
// * For each N a fast hash Fn is generated based on S and a chunk of memory from G that corresponds to N.
// * If for a set of N all Fn meet the difficulty criteria then we have a valid block.
// 
// Considerations:
// * F should be substantially slower than S such that T(S) >= (T(F)*Y)
// * Assuming 'T(S) >= (T(F)*Y)' it can be shown that when N=2 hashing with G/2 memory instead of the full memory will result in a slowdown such that only 1/4 of the hashes can be generated in the same time, for greater N the slow down would be greater..
// * Validation speed is also limited by 'F' however so 'F' should not be too slow either, a balance is required.
// * Y can also be reduced to balance this equation
// * Increasing N also increases the time penalty, but every extra N also means slower (full) validation. In some cases nodes could potentially randomly validate only parts of the hash bypassing part of this penalty.
//
// Munt specific details:
// Some extra details apply that are specific to our current implementation of SIGMA but could also be done differently:
// * We use a slightly modified 'argon' which we call 'argon_echo' to break compatibility with any argon specific chip implementations.
// * We use two different hash algorithms 'shavite' and 'echo' for the fast hashes, the choice of which to use is generated randomly along with the indices.
//   This is done to force extra branching with 50/50 probability and slow parallelisation on GPUs, as well as a way to force extra cost for ASIC implementations.
//   Both are chosen for their ability to use AES-ni (or other dedicated AES instructions) that most modern CPUs have to achieve performance that will be difficult to drastically beat on a GPU or ASIC.
// * We use AES as the PRNG - for similar reasons to above.
//
// There are various ways in which SIGMA can/could be adjusted the main parameters are outlined below:
// Arena size (currently 4gb):
// * The overall memory required by the algorithm for optimal generation performance.
// * The larger this is the less likely GPUs can perform the algorithm, and the more expensive and dominated by RAM price ASIC implementations are thus making them less competitive with CPUs.
// Argon memory cost (currently 4mb):
// * How much memory each individual argon hash uses.
// * Increasing this increases GPU resistance, but also increases verification requirements.
// Argon arena round cost (currently 8):
// * Increasing this affects how long the argon hashes for the initial arena takes (in conjunction with the memory cost)
// * Should be lower than the slow hash cost but high enough to still be secure
// Argon slow hash round cost (currently 12):
// * Increasing this affects how long the argon slow hashes take (in conjunction with the memory cost)
// * Longer is better, however also means longer verification
// Num pre-hashes (currently 65536):
// * Affects how many times the same global memory buffer can be hashed before a new one must be generated
// Num post-hashes (currently 40000):
// * Affects how many fast hashes we can perform per slow hash
// Fast hash size (currently 300 bytes):
// * Affects how much memory from the global buffer is consumed by each fast hash.
// * This ultimately affects the speed of the fast hash


// Consensus level SIGMA options and global defaults.
// NB! Must call verify after changing any settings.
class sigma_settings
{
public:
    sigma_settings();
    void verify();
public:
    uint64_t numVerifyThreads=4;         // Allow verification with 4 threads
    uint64_t arenaSizeKb=4*1024*1024;    // 4gb overall arena size
    uint64_t argonMemoryCostKb=4*1024;   // 4mb per arena/slow hash
    uint64_t argonArenaRoundCost=8;      // 8 rounds per arena hash
    uint64_t argonSlowHashRoundCost=12;  // 12 rounds per slow hash
    uint64_t numHashesPre=65536;         // 65536 pre-hashes
    uint64_t numHashesPost=65536;        // 65536 post-hashes 
    uint64_t fastHashSizeBytes=300;      // 300 bytes per fast hash
    
    // Calculated from other values in constructor.
    uint64_t arenaChunkSizeBytes=0;
    // Affects program behaviour (SIGMA activation block) not SIGMA itself.
    uint64_t activationDate=1571234400;
};
// Consensus level SIGMA defaults.
extern sigma_settings defaultSigmaSettings;


// We select the optimal implementation of these hash functions to match our CPU once at program start and then just use the function pointers throghout the SIGMA code.
void selectOptimisedImplementations();
extern uint64_t gSelShavite;
extern uint64_t gSelEcho;
extern uint64_t gSelArgon;
// Get a human readable name for the selections above
std::string selectedAlgorithmName(uint64_t nSel);

inline HashReturn (*selected_echo256_opt_Init)(echo256_opt_hashState* state) = nullptr;
inline HashReturn (*selected_echo256_opt_Update)(echo256_opt_hashState* state, const unsigned char* data, uint64_t databitlen) = nullptr;
inline HashReturn (*selected_echo256_opt_Final)(echo256_opt_hashState* state, unsigned char* hashval) = nullptr;
inline HashReturn (*selected_echo256_opt_UpdateFinal)(echo256_opt_hashState* state, unsigned char* hashval, const unsigned char* data, uint64_t databitlen) = nullptr;
inline bool (*selected_shavite3_256_opt_Init)(shavite3_256_opt_hashState* state) = nullptr;
inline bool (*selected_shavite3_256_opt_Update)(shavite3_256_opt_hashState* state, const unsigned char* data, uint64_t dataLenBytes) = nullptr;
inline bool (*selected_shavite3_256_opt_Final)(shavite3_256_opt_hashState* state, unsigned char* hashval) = nullptr;
inline int (*selected_argon2_echo_hash)(argon2_echo_context* context, bool doHash) = argon2_echo_ctx_ref;

void normaliseBufferSize(uint64_t& nBufferSizeBytes);

// Heavy weight sigma context for mining - allocated the entire arena (currently 4gb)
// NB!!! Take care creating/using these they allocate lots of memory..
class sigma_context
{
public:
    sigma_context(sigma_settings settings_, uint64_t allocateArenaSizeKb_, uint64_t numThreads_, uint64_t numArenaThreads_);
    bool arenaIsValid();
    void prepareArenas(CBlockHeader& headerData);
    void benchmarkSlowHashes(uint8_t* hashData, uint64_t numSlowHashes);
    void benchmarkFastHashes(uint8_t* hashData1, uint8_t* hashData2, uint8_t* hashData3, uint64_t numFastHashes);
    void benchmarkFastHashesRef(uint8_t* hashData1, uint8_t* hashData2, uint8_t* hashData3, uint64_t numFastHashes);
    void benchmarkMining(CBlockHeader& headerData, std::atomic<uint64_t>& slowHashCounter, std::atomic<uint64_t>& halfHashCounter, std::atomic<uint64_t>& skippedHashCounter, std::atomic<uint64_t>&hashCounter, std::atomic<uint64_t>&blockCounter, uint64_t nRoundsTarget);
    void mineBlock(CBlock* pBlock, std::atomic<uint64_t>& halfHashCounter, uint256& foundBlockHash, bool& interrupt);
    virtual ~sigma_context();
    sigma_context(const sigma_context&) = delete;
    sigma_context& operator=(const sigma_context&) = delete;
public:
    uint64_t numThreads=0;
    uint64_t numArenaThreads=0;
    uint64_t allocatedArenaSizeKb=0;
    uint8_t* arena=nullptr;
private:
    sigma_settings settings;
    uint64_t numHashesPossibleWithAvailableMemory=0;
};

// Light weight sigma context for header verification - allocates just the size of one argon round (16mb)
class sigma_verify_context
{
public:
    sigma_verify_context(sigma_settings settings_, uint64_t numUserVerifyThreads_);
    // Use verifyLevel to determine which of the fast hashes to check
    // 0 = Check both
    // 1 = Check first hash
    // 2 = Check second hash
    // NB! Do not use 1 or 2 unless you fully understand the repercussions; If in doubt stick to the default 0
    // Careless use of 1/2 can allow for carefully crafted attacks to split the chain.
    template<int verifyLevel=0> bool verifyHeader(CBlockHeader headerData);
    virtual ~sigma_verify_context();
    sigma_verify_context(const sigma_verify_context&) = delete;
    sigma_verify_context& operator=(const sigma_verify_context&) = delete;
private:
    sigma_settings settings;
    uint64_t numUserVerifyThreads;
    uint8_t* hashMem;
};

#endif
