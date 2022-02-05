#include <openssl/sha256.c>
#include "isa.h"

#include "sha256_vectors.h"

void digest_to_hex(unsigned char *digest, volatile char *hex)
{
	for (size_t i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		unsigned char tmp = digest[i] & 0xF;
		if (tmp < 0xA)
			hex[i*2 + 1] = '0' + tmp;
		else
			hex[i*2 + 1] = 'a' + tmp - 0xA;
		tmp = (digest[i] & 0xF0) >> 4;
		if (tmp < 0xA)
			hex[i*2] = '0' + tmp;
		else
			hex[i*2] = 'a' + tmp - 0xA;
	}
}

int main(int, char**)
{
	unsigned char *const digest = calloc(SHA256_DIGEST_LENGTH, 1);
	volatile char *const hex = calloc(2*SHA256_DIGEST_LENGTH, 1);

	SHA256_CTX ctx = { 0 };

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, tst_short, strlen(tst_short));
	SHA256_Final(digest, &ctx);

	digest_to_hex(digest, hex);

	SIM_ASSERT(strcmp((char *const)hex, tst_short_digest) == 0);
	SIM_PRINT("(short test correct)");
	
	SIM_PRINT("Digest of: ");
	SIM_PRINT(tst_short);
	SIM_PRINT("is");
	SIM_PRINT(hex);

	ctx = (SHA256_CTX) { 0 };

	SIM_BENCH_BEGIN("sha256");

	SHA256_Init(&ctx);
#ifdef QUICK
	SHA256_Update(&ctx, tst_long, 16384);
	SIM_BENCH_END();
	SIM_QUIT();
#else
	SHA256_Update(&ctx, tst_long, strlen(tst_long));
#endif
	SHA256_Final(digest, &ctx);

	digest_to_hex(digest, hex);

	for (int i = 0; i < 10; i++) { }

	SIM_PRINT("Digest of novel is: ");
	SIM_PRINT(hex);
	SIM_PRINT("Should be: ");
	SIM_PRINT(tst_long_digest);

	if (strcmp((char *const)hex, tst_long_digest) != 0) {
		SIM_PRINT("Long test wrong!");
		SIM_ASSERT(0);
	} else {
		SIM_PRINT("Long test correct!");
		SIM_BENCH_END();
		SIM_QUIT();
	}
}
