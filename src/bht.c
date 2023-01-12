#include "bht.h"

#include "util.h"

size_t bht_index(word_u pc, size_t global_history)
{
	if (feature_2level) {
		if (opt_gshare)
			return ((pc.u / 4u) ^ global_history) & BHT_INDEX_MASK;
		else
			return (((pc.u / 4u) << GLOBAL_HISTORY_BITS) | (global_history & GLOBAL_HISTORY_MASK)) & BHT_INDEX_MASK;
	} else {
		return (pc.u / 4u) & BHT_INDEX_MASK;
	}
}

uint8_t bht_update(const struct bht *curr, struct bht *next, word_u pc, size_t global_history, bool taken)
{
	size_t index = bht_index(pc, global_history);

	const bht_entry_t *old = &curr->buffer[index];
	bht_entry_t *e = &next->buffer[index];

	if (old->valid) {
		assert(old->debug_last_pc.u);
		if (old->debug_last_pc.u != pc.u) {
			tracei("[BHT] conflict: %x overwrites %x\n", pc.u, old->debug_last_pc.u);
		}

		uint8_t new_ctr;
		if (opt_1bitbht) {
			new_ctr = taken ? 3 : 0;
		} else {
			if (taken && old->ctr != 3) {
				new_ctr = old->ctr + 1;
			} else if (!taken && old->ctr != 0) {
				new_ctr = old->ctr - 1;
			} else {
				new_ctr = old->ctr;
			}
		}
		tracei("[BHT] %x %staken: %d -> %d\n", pc.u, taken ? "" : "not ", e->ctr, new_ctr);
		e->ctr = new_ctr;
	} else {
		e->valid = 1;
		e->ctr = taken ? 2 : 1;
		tracei("[BHT] %x %staken, initialised to %d\n", pc.u, taken ? "" : "not ", e->ctr);
	}
	e->debug_last_pc = pc;

	return e->ctr;
}

