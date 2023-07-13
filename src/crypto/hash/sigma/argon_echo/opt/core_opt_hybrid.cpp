// Copyright (c) 2019-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GNU Lesser General Public License v3, see the accompanying
// file COPYING

// This file is a thin wrapper around the actual 'argon2_echo_opt' implementation, along with various other similarly named files.
// The build system compiles each file with slightly different optimisation flags so that we have optimised implementations for a wide spread of processors.

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "../../../sigma/echo256/echo256_opt.h"
#include "../argon_echo.h"
#include "../core.h"

#include "../blake2/blamka-round-ref.h"
#include "../blake2/blake2-impl.h"
#include "../blake2/blake2.h"

#define next_addresses         next_addresses_hybrid
#define fill_segment             fill_segment_hybrid
#define fill_block                 fill_block_hybrid
#define initialize                 initialize_hybrid
#define fill_memory_blocks fill_memory_blocks_hybrid
#define argon2_echo_ctx       argon2_echo_ctx_hybrid
#define init_block_value     init_block_value_hybrid
#define copy_block                 copy_block_hybrid
#define xor_block                   xor_block_hybrid
#define finalize                     finalize_hybrid
#define fill_first_blocks   fill_first_blocks_hybrid
#define initial_hash             initial_hash_hybrid
#define fill_segment_ref         fill_segment_hybrid
#define fill_segment_thr     fill_segment_thr_hybrid
#define index_alpha               index_alpha_hybrid



#define ECHO_HASH_256(DATA, DATABYTELEN, HASH)                                         \
{                                                                                      \
    echo256_opt_hashState ctx_echo;                                                    \
    selected_echo256_opt_Init(&ctx_echo);                                              \
    selected_echo256_opt_Update(&ctx_echo, (const unsigned char*)(DATA), DATABYTELEN); \
    selected_echo256_opt_Final(&ctx_echo, HASH);                                       \
}

#define ARGON2_CORE_REF_IMPL
#include "../core.cpp"
#include "../argon2.cpp"

/*
 * Function fills a new memory block and optionally XORs the old block over the new one.
 * @next_block must be initialized.
 * @param prev_block Pointer to the previous block
 * @param ref_block Pointer to the reference block
 * @param next_block Pointer to the block to be constructed
 * @param with_xor Whether to XOR into the new block (1) or just overwrite (0)
 * @pre all block pointers must be valid
 */
static void fill_block_hybrid(const argon2_echo_block *prev_block, const argon2_echo_block *ref_block, argon2_echo_block *next_block, int with_xor)
{
    argon2_echo_block blockR, block_tmp;
    unsigned i;

    copy_block(&blockR, ref_block);
    xor_block(&blockR, prev_block);
    copy_block(&block_tmp, &blockR);
    /* Now blockR = ref_block + prev_block and block_tmp = ref_block + prev_block */
    if (with_xor)
    {
        /* Saving the next block contents for XOR over: */
        xor_block(&block_tmp, next_block);
        /* Now blockR = ref_block + prev_block and block_tmp = ref_block + prev_block + next_block */
    }

    /* Apply Blake2 on columns of 64-bit words: (0,1,...,15) , then (16,17,..31)... finally (112,113,...127) */
    for (i = 0; i < 8; ++i)
    {
        BLAKE2_ROUND_NOMSG_REF(
            blockR.v[16 * i], blockR.v[16 * i + 1], blockR.v[16 * i + 2],
            blockR.v[16 * i + 3], blockR.v[16 * i + 4], blockR.v[16 * i + 5],
            blockR.v[16 * i + 6], blockR.v[16 * i + 7], blockR.v[16 * i + 8],
            blockR.v[16 * i + 9], blockR.v[16 * i + 10], blockR.v[16 * i + 11],
            blockR.v[16 * i + 12], blockR.v[16 * i + 13], blockR.v[16 * i + 14],
            blockR.v[16 * i + 15]);
    }

    /* Apply Blake2 on rows of 64-bit words: (0,1,16,17,...112,113), then (2,3,18,19,...,114,115).. finally (14,15,30,31,...,126,127) */
    for (i = 0; i < 8; i++)
    {
        BLAKE2_ROUND_NOMSG_REF(
            blockR.v[2 * i], blockR.v[2 * i + 1], blockR.v[2 * i + 16],
            blockR.v[2 * i + 17], blockR.v[2 * i + 32], blockR.v[2 * i + 33],
            blockR.v[2 * i + 48], blockR.v[2 * i + 49], blockR.v[2 * i + 64],
            blockR.v[2 * i + 65], blockR.v[2 * i + 80], blockR.v[2 * i + 81],
            blockR.v[2 * i + 96], blockR.v[2 * i + 97], blockR.v[2 * i + 112],
            blockR.v[2 * i + 113]);
    }

    copy_block(next_block, &block_tmp);
    xor_block(next_block, &blockR);
}

static void next_addresses_hybrid(argon2_echo_block *address_block, argon2_echo_block *input_block, const argon2_echo_block *zero_block)
{
    input_block->v[6]++;
    fill_block_hybrid(zero_block, input_block, address_block, 0);
    fill_block_hybrid(zero_block, address_block, address_block, 0);
}

void fill_segment_hybrid(const argon2_echo_instance_t *instance, argon2_echo_position_t position)
{
    argon2_echo_block *ref_block = NULL, *curr_block = NULL;
    argon2_echo_block address_block, input_block, zero_block;
    uint64_t pseudo_rand, ref_index, ref_lane;
    uint32_t prev_offset, curr_offset;
    uint32_t starting_index;
    uint32_t i;
    
    //Always false for Argon2d
    bool data_independent_addressing = false;

    if (instance == NULL)
    {
        return;
    }

    //data_independent_addressing = (instance->type == Argon2_i) || (instance->type == Argon2_id && (position.pass == 0) && (position.slice < ARGON2_SYNC_POINTS / 2));

    if (data_independent_addressing)
    {
        init_block_value(&zero_block, 0);
        init_block_value(&input_block, 0);

        input_block.v[0] = position.pass;
        input_block.v[1] = position.lane;
        input_block.v[2] = position.slice;
        input_block.v[3] = instance->memory_blocks;
        input_block.v[4] = instance->passes;
        //input_block.v[5] = instance->type;
    }

    starting_index = 0;

    if ((0 == position.pass) && (0 == position.slice))
    {
        starting_index = 2; /* we have already generated the first two blocks */

        /* Don't forget to generate the first block of addresses: */
        if (data_independent_addressing)
        {
            next_addresses_hybrid(&address_block, &input_block, &zero_block);
        }
    }

    /* Offset of the current block */
    curr_offset = position.lane * instance->lane_length + position.slice * instance->segment_length + starting_index;

    if (0 == curr_offset % instance->lane_length)
    {
        /* Last block in this lane */
        prev_offset = curr_offset + instance->lane_length - 1;
    }
    else
    {
        /* Previous block */
        prev_offset = curr_offset - 1;
    }

    for (i = starting_index; i < instance->segment_length; ++i, ++curr_offset, ++prev_offset)
    {
        /*1.1 Rotating prev_offset if needed */
        if (curr_offset % instance->lane_length == 1)
        {
            prev_offset = curr_offset - 1;
        }

        /* 1.2 Computing the index of the reference block */
        /* 1.2.1 Taking pseudo-random value from the previous block */
        if (data_independent_addressing)
        {
            if (i % ARGON2_ADDRESSES_IN_BLOCK == 0)
            {
                next_addresses_hybrid(&address_block, &input_block, &zero_block);
            }
            pseudo_rand = address_block.v[i % ARGON2_ADDRESSES_IN_BLOCK];
        }
        else
        {
            pseudo_rand = instance->memory[prev_offset].v[0];
        }

        /* 1.2.2 Computing the lane of the reference block */
        ref_lane = ((pseudo_rand >> 32)) % instance->lanes;

        if ((position.pass == 0) && (position.slice == 0))
        {
            /* Can not reference other lanes yet */
            ref_lane = position.lane;
        }

        /* 1.2.3 Computing the number of possible reference block within the
         * lane.
         */
        position.index = i;
        ref_index = index_alpha(instance, &position, pseudo_rand & 0xFFFFFFFF, ref_lane == position.lane);

        /* 2 Creating a new block */
        ref_block = instance->memory + instance->lane_length * ref_lane + ref_index;
        curr_block = instance->memory + curr_offset;

        if(0 == position.pass)
        {
            fill_block_hybrid(instance->memory + prev_offset, ref_block, curr_block, 0);
        }
        else
        {
            fill_block_hybrid(instance->memory + prev_offset, ref_block, curr_block, 1);
        }
    }
}
