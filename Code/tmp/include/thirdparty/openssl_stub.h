#pragma once

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct evp_md_st EVP_MD;

int RAND_bytes(unsigned char* buf, int num);
const EVP_MD* EVP_sha256(void);
int PKCS5_PBKDF2_HMAC(const char* pass, int passlen,
                      const unsigned char* salt, int saltlen,
                      int iter, const EVP_MD* digest,
                      int keylen, unsigned char* out);
int CRYPTO_memcmp(const void* a, const void* b, size_t len);

#ifdef __cplusplus
}
#endif

