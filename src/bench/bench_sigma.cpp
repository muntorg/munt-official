// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "util.h"
#include <crypto/hash/sigma/sigma.h>
#include <iostream>
#include <boost/program_options.hpp>
#include <thread>

using namespace boost::program_options;
int LogPrintStr(const std::string &str)
{
    std::cout << str;
    return 1;
}

void selectLargesHashUnit(double& nSustainedHashesPerSecond, std::string& label)
{
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "kh";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "mh";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "gh";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "th";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "ph";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "eh";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "zh";
        nSustainedHashesPerSecond /= 1000.0;
    }
    if (nSustainedHashesPerSecond > 1000)
    {
        label = "yh";
        nSustainedHashesPerSecond /= 1000.0;
    }
}

double calculateSustainedHashrateForTimePeriod(uint64_t maxHashesPre, uint64_t maxHashesPost, double nHalfHashAverage, uint64_t nArenaSetuptime, uint64_t nTimePeriodSeconds)
{
    // We need to recalculate the arenas every time we exhaust the hash space, or when a new block comes in, whichever comes first.
    // Block target is 2.5 minutes, so we assume that the maximum time we can mine in between arena calculations is 3 minutes,
    // or the full hash space, whichever is lower.
    uint64_t maxHalfHashes = maxHashesPre*maxHashesPost;
    maxHalfHashes = std::min(maxHalfHashes, (uint64_t)((nTimePeriodSeconds*1000000)/nHalfHashAverage));
    
    // The total time is then the arena setup time plus the time that calculating maxHalfHashes would take (all in microseconds)
    uint64_t nTimeTotal = nArenaSetuptime + (maxHalfHashes*nHalfHashAverage);
    
    // Finally we can get a sustained hashrate by calculating the time per hash and then calculating how many hashes per second.
    double nSustainedMicrosecondsPerHash = ((double)nTimeTotal / (double)maxHalfHashes);
    double nSustainedHashesPerMicrosecond = (1/nSustainedMicrosecondsPerHash);
    return nSustainedHashesPerMicrosecond * 1000000;
}
    
int main(int argc, char** argv)
{
    srand(GetTimeMicros());

    // SIGMA paramaters (centrally set by network)
    uint64_t arenaCpuCostRounds = 8;
    uint64_t slowHashCpuCostRounds = 12;
    uint64_t memCostGb = 4;   
    uint64_t slowHashMemCostMb = 16;
    uint64_t fastHashMemCostBytes = 300;
    uint64_t maxHashesPre = 65536;
    uint64_t maxHashesPost = 65536;
    uint64_t numSigmaVerifyThreads = 4;
    bool defaultSigma = true;
    
    // User paramaters (adjustable on individual machines)
    uint64_t numThreads = std::thread::hardware_concurrency();
    uint64_t memAllowGb = memCostGb;
    uint64_t numUserVerifyThreads = std::min(numSigmaVerifyThreads, (uint64_t)std::thread::hardware_concurrency());
    uint64_t numFullHashesTarget = 50000;
    bool mineOnly=false;
    
    // Declare the supported options.
    options_description desc("Allowed options");
    desc.add_options()
    ("help", "produce help message")
    ("mine_threads", value<int64_t>(), "Set number of threads to use for mining")
    ("mine_memory", value<int64_t>(), "Set how much memory in gb to mine with")
    ("mine_num_hashes", value<int64_t>(), "How many full hash attempts to run mining for (default 50000)")
    ("mine_only", value<bool>(), "Only benchmark actual mining, skip other benchmarks")
    ("verify_threads", value<int64_t>(), "How many threads to use for verification, may not exceed sigma_verify_threads (defaults to same as sigma_verify_threads)")
    ("sigma_global_mem", value<int64_t>(), "How much global memory optimal mining should require (in gigabytes)")
    ("sigma_num_slow", value<int64_t>(), "How many slow hash attempts to allow for each global memory allocation  (maximum 65536)")
    ("sigma_slowhash_mem", value<int64_t>(), "How much memory each slow hash should consume (in megabytes)")
    ("sigma_slowhash_cpucost", value<int64_t>(), "How many rounds of computation to use in the argon slow hash computation (default 12)")
    ("sigma_arena_cpucost", value<int64_t>(), "How many rounds of computation to use in the arena hash computation (default 8)")
    ("sigma_num_fast", value<int64_t>(), "How many fast hash attempts to allow for each slow hash (maximum 65536)")
    ("sigma_fasthash_mem", value<int64_t>(), "How much of the global memory to digest for each slow hash (bytes - should not exceed the size of a single slow hash)")
    ("sigma_verify_threads", value<int64_t>(), "How many threads to allow for slow hashes and therefore verification. (Default 4)");
    
    
    variables_map vm;
    try
    {
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);    
    }
    catch (std::exception& e)
    {
        LogPrintf("%s\n", e.what());
        LogPrintf("%s", desc);
        return 1;
    }

    if (vm.size() == 0)
    {
        LogPrintf("Using default options use '--help' to see a list of possible options.\n\n");
    }
    else
    {
        LogPrintf("Using non default options use '--help' to see a list of possible options.\n\n");
    }
    
    if (vm.count("help"))
    {
        LogPrintf("%s", desc);
        return 1;
    }

    if (vm.count("mine_threads"))
    {
        numThreads = vm["mine_threads"].as<int64_t>();
    }
    if (vm.count("mine_memory"))
    {
        memAllowGb = vm["mine_memory"].as<int64_t>();
    }
    if (vm.count("mine_num_hashes"))
    {
        numFullHashesTarget = vm["mine_num_hashes"].as<int64_t>();
    }
    if (vm.count("mine_only"))
    {
        mineOnly = vm["mine_only"].as<bool>();
        defaultSigma = false;
    }
    if (vm.count("verify_threads"))
    {
        numUserVerifyThreads = vm["verify_threads"].as<int64_t>();
    }
    if (vm.count("sigma_global_mem"))
    {
        memCostGb = vm["sigma_global_mem"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma_num_slow"))
    {
        defaultSigma = false;
        maxHashesPre = vm["sigma_num_slow"].as<int64_t>();
    }
    if (vm.count("sigma_slowhash_mem"))
    {
        slowHashMemCostMb = vm["sigma_slowhash_mem"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma_arena_cpucost"))
    {
        arenaCpuCostRounds = vm["sigma_arena_cpucost"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma_slowhash_cpucost"))
    {
        slowHashCpuCostRounds = vm["sigma_slowhash_cpucost"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma_num_fast"))
    {
        maxHashesPost = vm["sigma_num_fast"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma_fasthash_mem"))
    {
        fastHashMemCostBytes = vm["sigma_fasthash_mem"].as<int64_t>();
        defaultSigma = false;
    }
    if (vm.count("sigma_verify_threads"))
    {
        numSigmaVerifyThreads = vm["sigma_verify_threads"].as<int64_t>();
        defaultSigma = false;
    }
    
    
    if (numUserVerifyThreads > numSigmaVerifyThreads)
    {
        LogPrintf("Number of user verify threads may not exceed number of sigma verify threads");
        return 1;
    }
    
    LogPrintf("Configuration=====================================================\n\n");
    LogPrintf("NETWORK:\nGlobal memory cost [%dgb]\nArgon_echo cpu cost for arenas [%d rounds]\nArgon_echo cpu cost for slow hash [%d rounds]\nArgon_echo mem cost [%dMb]\nEcho/Shavite digest size [%d bytes]\nNumber of fast hashes per slow hash [%d]\nNumber of slow hashes per global arena [%d]\nNumber of verify threads [%d]\n\n", memCostGb, arenaCpuCostRounds ,slowHashCpuCostRounds, slowHashMemCostMb, fastHashMemCostBytes, maxHashesPost, maxHashesPre, numSigmaVerifyThreads);
    LogPrintf("USER:\nMining with [%d] threads\nMining with [%d gb] memory.\nVerifying with [%s] threads.\n\n", numThreads, memAllowGb, numUserVerifyThreads);
    
    // If we are using the default params then perform some tests to ensure everything runs the same across different machines
    if (defaultSigma)
    {
        LogPrintf("Tests=============================================================\n\n");
        
        CBlockHeader header;
        LogPrintf("Attempt to validate valid header 1\n");
        {
            std::vector<unsigned char> data = ParseHex("daa464600000000080a6d654b146a17abe8e9cca80f477653f0350000000000000000000000000006fcb97fbd03a00ae5e1113a4e84616fcb43227000000000000000000d244645dffff3f1f0e003c83");
            memcpy(&header.nVersion, &data[0], 80);
            sigma_context sigmaContext(arenaCpuCostRounds, slowHashCpuCostRounds, 1024*slowHashMemCostMb, 1024*1024*memCostGb, 1024*1024*std::min(memAllowGb, memCostGb), maxHashesPre, maxHashesPost, numThreads, numSigmaVerifyThreads, numUserVerifyThreads, fastHashMemCostBytes);
            if (!sigmaContext.verifyHeader(header, 325540099))
            {
                LogPrintf("✘\n");
                return 1;
            }
            LogPrintf("✔\n");
        }
        LogPrintf("Attempt to validate valid header 2\n");
        {
            std::vector<unsigned char> data = ParseHex("5a3f6d4e00000000000054c65c8ff213fc3c0df71dab3d5804620100000000000000000000000000c027f3654d83fe8556519b6e8989f89dc83501000000000000000000d145645dffff3f1f020025dd");
            memcpy(&header.nVersion, &data[0], 80);
            sigma_context sigmaContext(arenaCpuCostRounds, slowHashCpuCostRounds, 1024*slowHashMemCostMb, 1024*1024*memCostGb, 1024*1024*std::min(memAllowGb, memCostGb), maxHashesPre, maxHashesPost, numThreads, numSigmaVerifyThreads, numUserVerifyThreads, fastHashMemCostBytes);
            if (!sigmaContext.verifyHeader(header, 1219157114))
            {
                LogPrintf("✘\n");
                return 1;
            }
            LogPrintf("✔\n");
        }
        LogPrintf("Attempt to validate valid header 3\n");
        {
            std::vector<unsigned char> data = ParseHex("10a2f82700000000eebb8ad5074e2c34c5feca57e84c32e8353cb301000000000000000000000000005425426760715d666805b4904c4f3551b30a0000000000000000006848645dffff3f1f0500b71e");
            memcpy(&header.nVersion, &data[0], 80);
            sigma_context sigmaContext(arenaCpuCostRounds, slowHashCpuCostRounds, 1024*slowHashMemCostMb, 1024*1024*memCostGb, 1024*1024*std::min(memAllowGb, memCostGb), maxHashesPre, maxHashesPost, numThreads, numSigmaVerifyThreads, numUserVerifyThreads, fastHashMemCostBytes);
            if (!sigmaContext.verifyHeader(header, 1548801618))
            {
                LogPrintf("✘\n");
                return 1;
            }
            LogPrintf("✔\n");
        }
        LogPrintf("Attempt to validate invalid header 1\n");
        {
            std::vector<unsigned char> data = ParseHex("e3a9e279001100001e518f7f23b526c1eebcd14731253c4bcff35a0000000000000000000000000070b25754c4650ea64b3965ed65035c78a9be260000000000000000006b435c5dffff3f1f0a005943");
            memcpy(&header.nVersion, &data[0], 80);
            sigma_context sigmaContext(arenaCpuCostRounds, slowHashCpuCostRounds, 1024*slowHashMemCostMb, 1024*1024*memCostGb, 1024*1024*std::min(memAllowGb, memCostGb), maxHashesPre, maxHashesPost, numThreads, numSigmaVerifyThreads, numUserVerifyThreads, fastHashMemCostBytes);
            if (sigmaContext.verifyHeader(header, 1967513924))
            {
                LogPrintf("✘\n");
                return 1;
            }
            LogPrintf("✔\n");
        }
        LogPrintf("\n");
    }
    
    //Random header to benchmark with, we will randomly change it more throughout the tests.
    CBlockHeader header;
    header.nVersion = rand();
    header.hashPrevBlock = ArithToUint256(((((((((arith_uint256(rand()) << 8) * rand()) << 8) * rand()) << 8) * rand()) << 8) * rand()));
    header.hashMerkleRoot = ArithToUint256(((((((((arith_uint256(rand()) << 8) * rand()) << 8) * rand()) << 8) * rand()) << 8) * rand()));
    header.nTime = rand();
    header.nBits = rand();
    header.nNonce = rand();
    
    if (!mineOnly)
    {
        LogPrintf("Scrypt============================================================\n\n");
        uint256 hash;    
        {
            LogPrintf("Bench cost [single thread]:\n");
            uint64_t nStart = GetTimeMicros(); 
            uint64_t numHashes = 20;
            char scratchpad[SCRYPT_SCRATCHPAD_SIZE];
            for (uint64_t i=0; i< numHashes; ++i)
            {
                header.nNonce = i;
                scrypt_1024_1_1_256_sp(BEGIN(header.nVersion), BEGIN(hash), scratchpad);
            }
            LogPrintf("total [%.2f micros] per hash: [%.2f micros]\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)numHashes);
        }
        
        {
            LogPrintf("Bench cost [%d threads]:\n", numThreads);
            uint64_t nStart = GetTimeMicros();
            uint64_t numHashes = 100;
            auto workerThreads = new boost::asio::thread_pool(numThreads);
            for (uint64_t i = 0; i <= numHashes;++i)
            {
                boost::asio::post(*workerThreads, [=]() mutable
                {
                    char scratchpad[SCRYPT_SCRATCHPAD_SIZE];
                    header.nNonce = i;
                    scrypt_1024_1_1_256_sp(BEGIN(header.nVersion), BEGIN(hash), scratchpad);    
                });
            }
            workerThreads->join();
            LogPrintf("total [%.2f micros] per hash [%.2f micros]\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)numHashes);
        }
        
        LogPrintf("SIGMA=============================================================\n\n");
        {
            LogPrintf("Bench slow hashes [single thread]:\n");
            sigma_context sigmaContext(arenaCpuCostRounds, slowHashCpuCostRounds, 1024*slowHashMemCostMb, 1024*1024*memCostGb, 1024*1024*std::min(memAllowGb, memCostGb), maxHashesPre, maxHashesPost, numThreads, numSigmaVerifyThreads, numUserVerifyThreads, fastHashMemCostBytes);
            
            {
                uint8_t hashData[80];
                for (int i=0;i<80;++i)
                {
                    hashData[i] = rand();
                }
                    
                uint64_t nStart = GetTimeMicros();
                uint64_t numSlowHashes = 100;
                sigmaContext.benchmarkSlowHashes(hashData, numSlowHashes);
                LogPrintf("total [%.2f micros] per hash [%.2f micros]\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)numSlowHashes);
            }
            
            {
                LogPrintf("Bench fast hashes [single thread]:\n");
                uint8_t hashData1[80];
                for (int i=0;i<80;++i)
                {
                    hashData1[i] = rand();
                }
                uint8_t hashData2[32];
                for (int i=0;i<32;++i)
                {
                    hashData2[i] = rand();
                }
                std::vector<unsigned char> hashData3(sigmaContext.fastHashSizeBytes);
                for (uint64_t i=0;i<sigmaContext.fastHashSizeBytes;++i)
                {
                    hashData3[i] = rand();
                }
                uint64_t nStart = GetTimeMicros();
                uint64_t numFastHashes = 100000;
                sigmaContext.benchmarkFastHashes(hashData1, hashData2, &hashData3[0], numFastHashes);
                LogPrintf("total [%.2f micros] per hash [%.4f micros]\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)numFastHashes);
            }
            
            {
                LogPrintf("Bench global arena priming [cpu_cost %drounds] [mem_cost %dgb]:\n", arenaCpuCostRounds, memCostGb );
                uint64_t nStart = GetTimeMicros(); 
                uint64_t numArenas=4;
                for (uint64_t i=0; i<numArenas; ++i)
                {
                    sigmaContext.prepareArenas(header, rand());
                }
                LogPrintf("total [%.2f micros] per round: [%.2f micros]\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)numArenas);
            }
            
            {
                LogPrintf("Bench verify [single thread]\n");
                uint64_t nVerifyNumber=100;
                uint64_t nCountValid=0;
                uint64_t nStart = GetTimeMicros();
                for (uint64_t i =0; i< nVerifyNumber; ++i)
                {
                    header.nNonce = rand();
                    nStart = GetTimeMicros();
                    // Count and log number of successes to avoid possibility of compiler optimising the call out.
                    if (sigmaContext.verifyHeader(header, rand()))
                    {
                        ++nCountValid;
                    }
                }
                LogPrintf("total [%.2f micros] per verification [%.2f micros] found [%d] valid random hashes\n\n", (GetTimeMicros() - nStart), ((GetTimeMicros() - nStart)) / (double)nVerifyNumber, nCountValid);
            }
        }
    }
    
    uint64_t nMineStart = GetTimeMicros();
    {
        if (mineOnly)
        {
            LogPrintf("SIGMA=============================================================\n\n");
        }
        header.nTime = GetTime();
        header.nVersion = rand();
        header.nBits = arith_uint256((~arith_uint256(0) >> 10)).GetCompact();
        
        std::vector<sigma_context*> sigmaContexts;
        std::vector<uint64_t> sigmaMemorySizes;
        uint64_t nMemoryAllocated=0;
        
        while (nMemoryAllocated < memAllowGb)
        {
            uint64_t nMemoryChunk = std::min((memAllowGb-nMemoryAllocated), memCostGb);
            nMemoryAllocated += nMemoryChunk;
            sigmaMemorySizes.emplace_back(nMemoryChunk);
        }
        for (auto instanceMemorySize : sigmaMemorySizes)
        {
            sigmaContexts.push_back(new sigma_context(arenaCpuCostRounds, slowHashCpuCostRounds, 1024*slowHashMemCostMb, 1024*1024*memCostGb, 1024*1024*instanceMemorySize, maxHashesPre, maxHashesPost, numThreads/sigmaMemorySizes.size(), numSigmaVerifyThreads, numUserVerifyThreads, fastHashMemCostBytes));
        }
        
        LogPrintf("Bench mining for low difficulty target\n");
        uint64_t nStart = GetTimeMicros();
        std::atomic<uint64_t> slowHashCounter = 0;
        std::atomic<uint64_t> halfHashCounter = 0;
        std::atomic<uint64_t> skippedHashCounter = 0;
        std::atomic<uint64_t> hashCounter = 0;
        std::atomic<uint64_t> blockCounter = 0;
        {
            auto workerThreads = new boost::asio::thread_pool(numThreads);
            for (auto sigmaContext : sigmaContexts)
            {
                boost::asio::post(*workerThreads, [&, header, sigmaContext]() mutable
                {
                    sigmaContext->prepareArenas(header, rand());
                });
            }
            workerThreads->join();
        }
        double nHalfHashAverage=0;
        uint64_t nArenaSetuptime = (GetTimeMicros() - nStart);
        LogPrintf("Arena setup time [%.2f micros]\n", nArenaSetuptime);
        nStart = GetTimeMicros();
        {
            auto workerThreads = new boost::asio::thread_pool(numThreads);
            for (auto sigmaContext : sigmaContexts)
            {
                boost::asio::post(*workerThreads, [&, header, sigmaContext]() mutable
                {
                    sigmaContext->benchmarkMining(header, slowHashCounter, halfHashCounter, skippedHashCounter, hashCounter, blockCounter, numFullHashesTarget);
                });
            }
            workerThreads->join();
        }
        nHalfHashAverage = ((GetTimeMicros() - nStart)) / (double)halfHashCounter;
        LogPrintf("slow-hashes [%d] half-hashes[%d] skipped-hashes [%d] full-hashes [%d] blocks [%d] total [%.2f micros] per half-hash[%.2f micros] per hash [%.2f micros]\n\n", slowHashCounter, halfHashCounter, skippedHashCounter, hashCounter, blockCounter, (GetTimeMicros() - nStart), nHalfHashAverage, ((GetTimeMicros() - nStart)) / (double)hashCounter);
        
        
        double nSustainedHashesPerSecond = calculateSustainedHashrateForTimePeriod(maxHashesPre, maxHashesPost, nHalfHashAverage, nArenaSetuptime, 150);
        double nSustainedHashesPerSecond30s = calculateSustainedHashrateForTimePeriod(maxHashesPre, maxHashesPost, nHalfHashAverage, nArenaSetuptime, 30);
        double nSustainedHashesPerSecond60s = calculateSustainedHashrateForTimePeriod(maxHashesPre, maxHashesPost, nHalfHashAverage, nArenaSetuptime, 60);
        
        // Convert to the largest unit we can so that output is easier to read
        std::string labelSustained = " h";        
        selectLargesHashUnit(nSustainedHashesPerSecond, labelSustained);
        std::string labelSustained30s = " h";        
        selectLargesHashUnit(nSustainedHashesPerSecond30s, labelSustained30s);
        std::string labelSustained60s = " h";        
        selectLargesHashUnit(nSustainedHashesPerSecond60s, labelSustained60s);
        
        // Log a highly noticeable number for users who just want a number to compare without all the gritty details.
        
        LogPrintf("\n================================================");
        LogPrintf("\n* Estimated 30s hashrate %16.2f %s/s *", nSustainedHashesPerSecond30s, labelSustained30s);
        LogPrintf("\n* Estimated 1m hashrate %17.2f %s/s *", nSustainedHashesPerSecond60s, labelSustained60s);
        LogPrintf("\n* Estimated sustained hashrate %10.2f %s/s *", nSustainedHashesPerSecond, labelSustained);
        LogPrintf("\n================================================\n");
    }
    
    uint64_t nMineEnd = GetTimeMicros();
    LogPrintf("\nBenchmarks finished in [%d seconds]\n", (nMineEnd-nMineStart)*0.000001);
    
    if ((nMineEnd-nMineStart)*0.000001< 30)
    {
        // Calculate hash target to spend 200 seconds running and suggest user set that.
        // NB! We delibritely test for 30 but calculate on 200 to prevent making people run the program multiple times unnecessarily.
        LogPrintf("Mining benchmark too fast to be accurate recommend running with `--mine_num_hashes=%d` or larger for at least 200 seconds of benchmarking.\n", (uint64_t)(numFullHashesTarget*((200000000)/(nMineEnd-nMineStart))));
    }
    //NB! We leak sigmaContexts here, we don't really care because this is a trivial benchmark program its faster for the user to just exit than to actually free them.
}
