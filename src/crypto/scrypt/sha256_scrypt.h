#include <assert.h>
#include <compat/endian.h>
#include <support/cleanse.h>

/* Context structure for SHA256 operations. */
typedef struct {
	uint32_t state[8];
	uint64_t count;
	uint8_t buf[64];
} SHA256_CTX;

/* Context structure for HMAC-SHA256 operations. */
typedef struct {
	SHA256_CTX ictx;
	SHA256_CTX octx;
} HMAC_SHA256_CTX;

/* SHA256 round constants. */
static const uint32_t Krnd[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};


/*
 * SHA256 block compression function.  The 256-bit state is transformed via
 * the 512-bit input block to produce a new state.
 */
static void SHA256_Transform(uint32_t state[8], const uint8_t block[64], uint32_t W[64], uint32_t S[8]);

/**
 * SHA256_Init(ctx):
 * Initialize the SHA256 context ${ctx}.
 */
void SHA256_Init(SHA256_CTX * ctx);

/**
 * SHA256_Update(ctx, in, len):
 * Input ${len} bytes from ${in} into the SHA256 context ${ctx}.
 */
static void _SHA256_Update(SHA256_CTX * ctx, const void * in, size_t len, uint32_t tmp32[72]);

/* Wrapper function for intermediate-values sanitization. */
void SHA256_Update(SHA256_CTX * ctx, const void * in, size_t len);

/* Add padding and terminating bit-count. */
static void SHA256_Pad(SHA256_CTX * ctx, uint32_t tmp32[72]);

/**
 * SHA256_Final(digest, ctx):
 * Output the SHA256 hash of the data input to the context ${ctx} into the
 * buffer ${digest}.
 */
static void _SHA256_Final(uint8_t digest[32], SHA256_CTX * ctx, uint32_t tmp32[72]);

/* Wrapper function for intermediate-values sanitization. */
void SHA256_Final(uint8_t digest[32], SHA256_CTX * ctx);

/* Initialize an HMAC-SHA256 operation with the given key. */
static void HMAC_SHA256_Init(HMAC_SHA256_CTX *ctx, const void *_K, size_t Klen);

/* Add bytes to the HMAC-SHA256 operation. */
static void HMAC_SHA256_Update(HMAC_SHA256_CTX *ctx, const void *in, size_t len);

/* Finish an HMAC-SHA256 operation. */
static void HMAC_SHA256_Final(unsigned char digest[32], HMAC_SHA256_CTX *ctx);

/**
 * PBKDF2_SHA256(passwd, passwdlen, salt, saltlen, c, buf, dkLen):
 * Compute PBKDF2(passwd, salt, c, dkLen) using HMAC-SHA256 as the PRF, and
 * write the output to buf.  The value dkLen must be at most 32 * (2^32 - 1).
 */
void PBKDF2_SHA256(const uint8_t *passwd, size_t passwdlen, const uint8_t *salt, size_t saltlen, uint64_t c, uint8_t *buf, size_t dkLen);
