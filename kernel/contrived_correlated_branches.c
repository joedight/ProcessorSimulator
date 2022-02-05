#include "isa.h"
#include "sort_vectors.h"

int compare_char(const void *a, const void *b, void *)
{
	char x = *(char*)a;
	char y = *(char*)b;
	if (x < y)
		return -1;
	if (x == y)
		return 0;
	else
		return 1;
}

int main(int argc, char **argv)
{
	volatile size_t ctr1 = 0;
	volatile size_t ctr2 = 0;

	SIM_BENCH_BEGIN("Contrived branches");
	bool last_loop = false;
	for (size_t x = 0; x < 100; x++) {
		for (size_t i = 1; i < 127; i++) {
			bool x = compare_char(&test_short[i], &test_short[i+1], NULL) == 1;
			if (x)
				ctr1++;
			if (x && last_loop)
				ctr2++;
			last_loop = x;
		}
	}
	SIM_BENCH_END();

	SIM_QUIT();
}
