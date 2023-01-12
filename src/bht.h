/* Branch History Table */
#pragma once

#include "word.h"
#include "config.h"

typedef struct {
	bool valid;
	uint8_t ctr;

	word_u debug_last_pc;
} bht_entry_t;

struct bht {
	bht_entry_t buffer[BHT_SIZE];
};

/* Mapping pc * history -> bht_index. */
size_t bht_index(word_u pc, size_t global_history);

uint8_t bht_update(const struct bht *curr, struct bht *next, word_u pc, size_t global_history, bool taken);

