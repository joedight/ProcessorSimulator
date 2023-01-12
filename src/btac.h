/* BTAC */
#pragma once
#include "config.h"
#include "word.h"
typedef struct {
	word_u br_pc;
	word_u taddr;
} btac_entry_t;

struct btac {
	btac_entry_t buffer[BTAC_SIZE];
};

/* Replace an entry in the BTAC. */
void btac_update(struct btac *next, word_u pc, word_u taddr);

