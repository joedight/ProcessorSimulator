#include "btac.h"

void btac_update(struct btac *next, word_u pc, word_u taddr)
{
	if (opt_nospec)
		return;

	if (taddr.u) {
		next->buffer[(pc.u / 4) & BTAC_INDEX_MASK].br_pc = pc;
	} else {
		next->buffer[(pc.u / 4) & BTAC_INDEX_MASK].br_pc = (word_u) { .u = 0 };
	}
	next->buffer[(pc.u / 4) & BTAC_INDEX_MASK].taddr = taddr;
}

