/*******************************************************************/
/*  SHA3api_ref.h -- Definitions required by the API               */
/*                                                                 */
/*  Written by Eli Biham and Orr Dunkelman                         */
/*                                                                 */
/*******************************************************************/

// definitions imposed by the API
#ifndef SHA3API_H
#define SHA3API_H

#include <stdint.h>

/* SHAvite-3 definition */
typedef struct
{
   uint64_t bitcount;            /* The number of bits compressed so far   */
   uint8_t chaining_value[64]; /* An array containing the chaining value */
   uint8_t buffer[128];        /* A buffer storing bytes until they are  */
				   /* compressed			     */
   uint8_t partial_byte;       /* A byte to store a fraction of a byte   */
				   /* in case the input is not fully byte    */
				   /* alligned				     */
   uint8_t salt[64];           /* The salt used in the hash function     */ 
   int DigestSize;		   /* The requested digest size              */
   int BlockSize;		   /* The message block size                 */
} hashState;

// Function calls imposed by the API
bool shavite3_ref_Init(hashState* state);
bool shavite3_ref_Update(hashState* state, const uint8_t* data, uint64_t databitlen);
bool shavite3_ref_Final(hashState* state, uint8_t* hashval);

#endif
