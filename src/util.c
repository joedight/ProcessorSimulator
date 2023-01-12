#include "util.h"

bool tracei_enabled = 1;

void tracei(char *fmt, ...)
{
	if (!tracei_enabled)
		return;

	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}
