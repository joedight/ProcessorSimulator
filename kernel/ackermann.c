#include <stdint.h>
#include <string.h>
#include "isa.h"

uint32_t ackermann(uint32_t m, uint32_t n)
{
	if (m == 0)
		return n+1;
	else if (n == 0)
		return ackermann(m-1, 1);
	else
		return ackermann(m-1, ackermann(m, n-1));
}

void itos(char *str, uint32_t i) {
	int x = strlen(str);
	while (0 != i) {
		str[--x] = '0' + (i % 10);
		i /= 10;
	}
}

int main(int argc, char **argv)
{
	uint32_t x;

	x = ackermann(2, 2);
	SIM_ASSERT(x == 7);

	x = ackermann(2, 4);
	SIM_ASSERT(x == 11);


	x = ackermann(3, 1);
	SIM_ASSERT(x == 13);


	x = ackermann(3, 4);
	SIM_ASSERT(x == 125);

	SIM_BENCH_BEGIN("ackermann");
	x = ackermann(3, 7);

	char *s = "ackermann(3, 7) is:      ";
	itos(s, x);
	SIM_PRINT(s);

	SIM_ASSERT(x == 1021);

	SIM_BENCH_END();

	SIM_QUIT();
}

