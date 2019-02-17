// -
// Copyright 2009 Colin Percival
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//
// The code in this file is taken from a file which was originally written by Colin Percival as part of the Tarsnap
// online backup system.
// -
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING in the root of this repository

#include "sha256_scrypt.h"

#include <assert.h>
#include "string.h"
#include <crypto/sha256.h>


void HMAC_SHA256_Init(HMAC_SHA256_CTX* ctx, const void* _K, size_t Klen)
{
    unsigned char pad[64];
    unsigned char khash[32];
    const unsigned char *K = (const unsigned char *)_K;
    size_t i;

    // If Klen > 64, the key is really SHA256(K).
    if (Klen > 64)
    {
        ctx->ictx.Reset();
        ctx->ictx.Write(K, Klen);
        ctx->ictx.Finalize(khash);
        K = khash;
        Klen = 32;
    }

    // Inner SHA256 operation is SHA256(K xor [block of 0x36] || data).
    ctx->ictx.Reset();
    memset(pad, 0x36, 64);
    for (i = 0; i < Klen; i++)
    {
        pad[i] ^= K[i];
    }
    ctx->ictx.Write(pad, 64);

    // Outer SHA256 operation is SHA256(K xor [block of 0x5c] || hash).
    ctx->octx.Reset();
    memset(pad, 0x5c, 64);
    for (i = 0; i < Klen; i++)
    {
        pad[i] ^= K[i];
    }
    ctx->octx.Write(pad, 64);

    // Clean the stack.
    memset(khash, 0, 32);
}

void HMAC_SHA256_Update(HMAC_SHA256_CTX* ctx, const void* in, size_t len)
{
    // Feed data to the inner SHA256 operation.
    ctx->ictx.Write((unsigned char*)in, len);
}

void HMAC_SHA256_Final(unsigned char digest[32], HMAC_SHA256_CTX* ctx)
{
    unsigned char ihash[32];

    // Finish the inner SHA256 operation.
    ctx->ictx.Finalize(ihash);

    // Feed the inner hash to the outer SHA256 operation.
    ctx->octx.Write(ihash, 32);

    // Finish the outer SHA256 operation.
    ctx->octx.Finalize(digest);

    // Clean the stack.
    memset(ihash, 0, 32);
}

void PBKDF2_SHA256(const uint8_t* passwd, size_t passwdlen, const uint8_t* salt, size_t saltlen, uint64_t c, uint8_t* buf, size_t dkLen)
{
    HMAC_SHA256_CTX PShctx, hctx;
    size_t i;
    uint8_t ivec[4];
    uint8_t U[32];
    uint8_t T[32];
    uint64_t j;
    int k;
    size_t clen;

    // Compute HMAC state after processing P and S.
    HMAC_SHA256_Init(&PShctx, passwd, passwdlen);
    HMAC_SHA256_Update(&PShctx, salt, saltlen);

    // Iterate through the blocks.
    for (i = 0; i * 32 < dkLen; i++)
    {
        // Generate INT(i + 1).
        be32enc(ivec, (uint32_t)(i + 1));

        // Compute U_1 = PRF(P, S || INT(i)).
        memcpy(&hctx, &PShctx, sizeof(HMAC_SHA256_CTX));
        HMAC_SHA256_Update(&hctx, ivec, 4);
        HMAC_SHA256_Final(U, &hctx);

        // T_i = U_1 ...
        memcpy(T, U, 32);

        for (j = 2; j <= c; j++)
        {
            // Compute U_j.
            HMAC_SHA256_Init(&hctx, passwd, passwdlen);
            HMAC_SHA256_Update(&hctx, U, 32);
            HMAC_SHA256_Final(U, &hctx);

            // ... xor U_j ...
            for (k = 0; k < 32; k++)
            {
                T[k] ^= U[k];
            }
        }

        // Copy as many bytes as necessary into buf.
        clen = dkLen - i * 32;
        if (clen > 32)
        {
            clen = 32;
        }
        memcpy(&buf[i * 32], T, clen);
    }

    // Clean PShctx, since we never called _Final on it.
    memset(&PShctx, 0, sizeof(HMAC_SHA256_CTX));
}
