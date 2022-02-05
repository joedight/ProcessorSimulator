#include "isa.h"

#include "sha256_vectors.h"
#include <stdlib.h>

int main(int, char**)
{
	SIM_BENCH_BEGIN("Binary GCD");
	
	register unsigned long g = 0;
	register const unsigned long a = 1353309109, b = 1253617031;
	register unsigned long u = a, v = b;
	while (((u & 1) == 0) && ((v & 1) == 0)) {
		u >>= 1;
		v >>= 1;
		++g;
	}
	while (u > 0) {
		if (0 == (u & 1))
			u >>= 1;
		else if (0 == (v & 1))
			v >>= 1;
		else if (u < v)
			v = (v - u) >> 1;
		else
			u = (u - v) >> 1;
	}
	register unsigned long out = v << g;
//	SIM_ASSERT((a % out) == 0);
//	SIM_ASSERT((b % out) == 0);
	SIM_BENCH_END();
	SIM_QUIT();
}
