#ifndef __TPAD_GCRYPT_HELPERS__
#define __TPAD_GCRYPT_HELPERS__

#include <gcrypt.h>

#define TPAD_GCRYPT_MINVERS "1.6.0"
#define TPAD_USING_SECURE_MEMORY
#define TPAD_SECMEM_SIZE (65536)

#define TPAD_HASH_ALG (GCRY_MD_WHIRLPOOL)
#define TPAD_HASH_SIZE ((512/8)*2)

int tpad_gcinit(char *vers);
//void tpad_gcerror(const char *what, gcry_error_t err, int exitcode);

#endif
