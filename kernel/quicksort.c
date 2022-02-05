#include <glibc/qsort.c>
#include "sort_vectors.h"
#include "isa.h"

static int compare_char(const void *a, const void *b, void *)
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

static int compare_ulong(const void *a, const void *b, void *)
{
	unsigned long x = *(unsigned long*)a;
	unsigned long y = *(unsigned long*)b;
	if (x < y)
		return -1;
	if (x == y)
		return 0;
	else
		return 1;
}

int main(int argc, char **argv)
{
	SIM_ASSERT(strlen(test_short) == strlen(test_short_result));

	_quicksort(test_short, strlen(test_short), sizeof(char), compare_char, NULL);
	SIM_ASSERT(strcmp(test_short, test_short_result) == 0);
	SIM_PRINT("Short sort correct.");

	volatile int x, y, z;
	x = x * y;
	
#define LONG_TEST_LEN 2048
	SIM_BENCH_BEGIN("Quicksort");
	// _quicksort(test_long, sizeof(test_long) / sizeof(*test_long), sizeof(*test_long), compare_ulong, NULL);
	_quicksort(test_long, LONG_TEST_LEN, sizeof(*test_long), compare_ulong, NULL);
	SIM_BENCH_END();

	unsigned long max = 0;
	for (size_t i = 0; i < LONG_TEST_LEN; i++) {
		if (test_long[i] > max)
			max = test_long[i];
		else
			SIM_ASSERT(max == test_long[i]);
	}
	SIM_PRINT("Long sort correct.");

	SIM_QUIT();
}
