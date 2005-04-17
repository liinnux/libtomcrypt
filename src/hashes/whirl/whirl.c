/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtomcrypt.org
 */

/** 
   @file whirl.c
   WHIRLPOOL (using their new sbox) hash function by Tom St Denis 
*/

#include "tomcrypt.h"

#ifdef WHIRLPOOL

const struct ltc_hash_descriptor whirlpool_desc =
{
    "whirlpool",
    11,
    64,
    64,

    /* DER encoding (not yet supported) */
    { 0x00 },
    0,

    &whirlpool_init,
    &whirlpool_process,
    &whirlpool_done,
    &whirlpool_test
};

/* the sboxes */
#include "whirltab.c"

/* get a_{i,j} */
#define GB(a,i,j) ((a[(i) & 7] >> (8 * (j))) & 255)

/* shortcut macro to perform three functions at once */
#define theta_pi_gamma(a, i)             \
    SB0(GB(a, i-0, 7)) ^                 \
    SB1(GB(a, i-1, 6)) ^                 \
    SB2(GB(a, i-2, 5)) ^                 \
    SB3(GB(a, i-3, 4)) ^                 \
    SB4(GB(a, i-4, 3)) ^                 \
    SB5(GB(a, i-5, 2)) ^                 \
    SB6(GB(a, i-6, 1)) ^                 \
    SB7(GB(a, i-7, 0))

#ifdef LTC_CLEAN_STACK
static int _whirlpool_compress(hash_state *md, unsigned char *buf)
#else
static int whirlpool_compress(hash_state *md, unsigned char *buf)
#endif
{
   ulong64 K[2][8], T[3][8];
   int x, y;
   
   /* load the block/state */
   for (x = 0; x < 8; x++) {
      K[0][x] = md->whirlpool.state[x];

      LOAD64H(T[0][x], buf + (8 * x));
      T[2][x]  = T[0][x];
      T[0][x] ^= K[0][x];
   }
  
   /* do rounds 1..10 */
   for (x = 0; x < 10; x += 2) {
       /* odd round */
       /* apply main transform to K[0] into K[1] */
       for (y = 0; y < 8; y++) {
           K[1][y] = theta_pi_gamma(K[0], y);
       }
       /* xor the constant */
       K[1][0] ^= cont[x];
       
       /* apply main transform to T[0] into T[1] */
       for (y = 0; y < 8; y++) {
           T[1][y] = theta_pi_gamma(T[0], y) ^ K[1][y];
       }

       /* even round */
       /* apply main transform to K[1] into K[0] */
       for (y = 0; y < 8; y++) {
           K[0][y] = theta_pi_gamma(K[1], y);
       }
       /* xor the constant */
       K[0][0] ^= cont[x+1];
       
       /* apply main transform to T[1] into T[0] */
       for (y = 0; y < 8; y++) {
           T[0][y] = theta_pi_gamma(T[1], y) ^ K[0][y];
       }
   }
   
   /* store state */
   for (x = 0; x < 8; x++) {
      md->whirlpool.state[x] ^= T[0][x] ^ T[2][x];
   }

   return CRYPT_OK;
}


#ifdef LTC_CLEAN_STACK
static int whirlpool_compress(hash_state *md, unsigned char *buf)
{
   int err;
   err = _whirlpool_compress(md, buf);
   burn_stack((5 * 8 * sizeof(ulong64)) + (2 * sizeof(int)));
   return err;
}
#endif


/**
   Initialize the hash state
   @param md   The hash state you wish to initialize
   @return CRYPT_OK if successful
*/
int whirlpool_init(hash_state * md)
{
   LTC_ARGCHK(md != NULL);
   zeromem(&md->whirlpool, sizeof(md->whirlpool));
   return CRYPT_OK;
}

/**
   Process a block of memory though the hash
   @param md     The hash state
   @param in     The data to hash
   @param inlen  The length of the data (octets)
   @return CRYPT_OK if successful
*/
HASH_PROCESS(whirlpool_process, whirlpool_compress, whirlpool, 64)

/**
   Terminate the hash to get the digest
   @param md  The hash state
   @param out [out] The destination of the hash (64 bytes)
   @return CRYPT_OK if successful
*/
int whirlpool_done(hash_state * md, unsigned char *out)
{
    int i;

    LTC_ARGCHK(md  != NULL);
    LTC_ARGCHK(out != NULL);

    if (md->whirlpool.curlen >= sizeof(md->whirlpool.buf)) {
       return CRYPT_INVALID_ARG;
    }

    /* increase the length of the message */
    md->whirlpool.length += md->whirlpool.curlen * 8;

    /* append the '1' bit */
    md->whirlpool.buf[md->whirlpool.curlen++] = (unsigned char)0x80;

    /* if the length is currently above 32 bytes we append zeros
     * then compress.  Then we can fall back to padding zeros and length
     * encoding like normal.
     */
    if (md->whirlpool.curlen > 32) {
        while (md->whirlpool.curlen < 64) {
            md->whirlpool.buf[md->whirlpool.curlen++] = (unsigned char)0;
        }
        whirlpool_compress(md, md->whirlpool.buf);
        md->whirlpool.curlen = 0;
    }

    /* pad upto 56 bytes of zeroes (should be 32 but we only support 64-bit lengths)  */
    while (md->whirlpool.curlen < 56) {
        md->whirlpool.buf[md->whirlpool.curlen++] = (unsigned char)0;
    }

    /* store length */
    STORE64H(md->whirlpool.length, md->whirlpool.buf+56);
    whirlpool_compress(md, md->whirlpool.buf);

    /* copy output */
    for (i = 0; i < 8; i++) {
        STORE64H(md->whirlpool.state[i], out+(8*i));
    }
#ifdef LTC_CLEAN_STACK
    zeromem(md, sizeof(*md));
#endif
    return CRYPT_OK;
}

/**
  Self-test the hash
  @return CRYPT_OK if successful, CRYPT_NOP if self-tests have been disabled
*/  
int  whirlpool_test(void)
{
 #ifndef LTC_TEST
    return CRYPT_NOP;
 #else    
  static const struct {
      int len;
      unsigned char msg[128], hash[64];
  } tests[] = {
  
  /* NULL Message */
{
  0, 
  { 0x00 },
  { 0x19, 0xFA, 0x61, 0xD7, 0x55, 0x22, 0xA4, 0x66, 0x9B, 0x44, 0xE3, 0x9C, 0x1D, 0x2E, 0x17, 0x26,
    0xC5, 0x30, 0x23, 0x21, 0x30, 0xD4, 0x07, 0xF8, 0x9A, 0xFE, 0xE0, 0x96, 0x49, 0x97, 0xF7, 0xA7,
    0x3E, 0x83, 0xBE, 0x69, 0x8B, 0x28, 0x8F, 0xEB, 0xCF, 0x88, 0xE3, 0xE0, 0x3C, 0x4F, 0x07, 0x57,
    0xEA, 0x89, 0x64, 0xE5, 0x9B, 0x63, 0xD9, 0x37, 0x08, 0xB1, 0x38, 0xCC, 0x42, 0xA6, 0x6E, 0xB3 }
},


   /* 448-bits of 0 bits */
{

  56,
  { 0x00 },
  { 0x0B, 0x3F, 0x53, 0x78, 0xEB, 0xED, 0x2B, 0xF4, 0xD7, 0xBE, 0x3C, 0xFD, 0x81, 0x8C, 0x1B, 0x03,
    0xB6, 0xBB, 0x03, 0xD3, 0x46, 0x94, 0x8B, 0x04, 0xF4, 0xF4, 0x0C, 0x72, 0x6F, 0x07, 0x58, 0x70,
    0x2A, 0x0F, 0x1E, 0x22, 0x58, 0x80, 0xE3, 0x8D, 0xD5, 0xF6, 0xED, 0x6D, 0xE9, 0xB1, 0xE9, 0x61,
    0xE4, 0x9F, 0xC1, 0x31, 0x8D, 0x7C, 0xB7, 0x48, 0x22, 0xF3, 0xD0, 0xE2, 0xE9, 0xA7, 0xE7, 0xB0 }
},

   /* 520-bits of 0 bits */
{
  65,
  { 0x00 },
  { 0x85, 0xE1, 0x24, 0xC4, 0x41, 0x5B, 0xCF, 0x43, 0x19, 0x54, 0x3E, 0x3A, 0x63, 0xFF, 0x57, 0x1D,
    0x09, 0x35, 0x4C, 0xEE, 0xBE, 0xE1, 0xE3, 0x25, 0x30, 0x8C, 0x90, 0x69, 0xF4, 0x3E, 0x2A, 0xE4,
    0xD0, 0xE5, 0x1D, 0x4E, 0xB1, 0xE8, 0x64, 0x28, 0x70, 0x19, 0x4E, 0x95, 0x30, 0xD8, 0xD8, 0xAF,
    0x65, 0x89, 0xD1, 0xBF, 0x69, 0x49, 0xDD, 0xF9, 0x0A, 0x7F, 0x12, 0x08, 0x62, 0x37, 0x95, 0xB9 }
},

   /* 512-bits, leading set */
{
  64,
  { 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  { 0x10, 0x3E, 0x00, 0x55, 0xA9, 0xB0, 0x90, 0xE1, 0x1C, 0x8F, 0xDD, 0xEB, 0xBA, 0x06, 0xC0, 0x5A,
    0xCE, 0x8B, 0x64, 0xB8, 0x96, 0x12, 0x8F, 0x6E, 0xED, 0x30, 0x71, 0xFC, 0xF3, 0xDC, 0x16, 0x94,
    0x67, 0x78, 0xE0, 0x72, 0x23, 0x23, 0x3F, 0xD1, 0x80, 0xFC, 0x40, 0xCC, 0xDB, 0x84, 0x30, 0xA6,
    0x40, 0xE3, 0x76, 0x34, 0x27, 0x1E, 0x65, 0x5C, 0xA1, 0x67, 0x4E, 0xBF, 0xF5, 0x07, 0xF8, 0xCB }
},

   /* 512-bits, leading set of second byte */
{
  64,
  { 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  { 0x35, 0x7B, 0x42, 0xEA, 0x79, 0xBC, 0x97, 0x86, 0x97, 0x5A, 0x3C, 0x44, 0x70, 0xAA, 0xB2, 0x3E,
    0x62, 0x29, 0x79, 0x7B, 0xAD, 0xBD, 0x54, 0x36, 0x5B, 0x54, 0x96, 0xE5, 0x5D, 0x9D, 0xD7, 0x9F,
    0xE9, 0x62, 0x4F, 0xB4, 0x22, 0x66, 0x93, 0x0A, 0x62, 0x8E, 0xD4, 0xDB, 0x08, 0xF9, 0xDD, 0x35,
    0xEF, 0x1B, 0xE1, 0x04, 0x53, 0xFC, 0x18, 0xF4, 0x2C, 0x7F, 0x5E, 0x1F, 0x9B, 0xAE, 0x55, 0xE0 }
},

   /* 512-bits, leading set of last byte */
{
  64,
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80 },
  { 0x8B, 0x39, 0x04, 0xDD, 0x19, 0x81, 0x41, 0x26, 0xFD, 0x02, 0x74, 0xAB, 0x49, 0xC5, 0x97, 0xF6,
    0xD7, 0x75, 0x33, 0x52, 0xA2, 0xDD, 0x91, 0xFD, 0x8F, 0x9F, 0x54, 0x05, 0x4C, 0x54, 0xBF, 0x0F,
    0x06, 0xDB, 0x4F, 0xF7, 0x08, 0xA3, 0xA2, 0x8B, 0xC3, 0x7A, 0x92, 0x1E, 0xEE, 0x11, 0xED, 0x7B,
    0x6A, 0x53, 0x79, 0x32, 0xCC, 0x5E, 0x94, 0xEE, 0x1E, 0xA6, 0x57, 0x60, 0x7E, 0x36, 0xC9, 0xF7 }
},
   
};

  int i;
  unsigned char tmp[64];
  hash_state md;

  for (i = 0; i < (int)(sizeof(tests)/sizeof(tests[0])); i++) {
      whirlpool_init(&md);
      whirlpool_process(&md, (unsigned char *)tests[i].msg, tests[i].len);
      whirlpool_done(&md, tmp);
      if (memcmp(tmp, tests[i].hash, 64) != 0) {
#if 0      
         printf("\nFailed test %d\n", i);
         for (i = 0; i < 64; ) {
            printf("%02x ", tmp[i]);
            if (!(++i & 15)) printf("\n");
         }
#endif         
         return CRYPT_FAIL_TESTVECTOR;
      }
  }
  return CRYPT_OK;
 #endif
}


#endif
