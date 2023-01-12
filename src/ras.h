#pragma once
#include "config.h"
#include "word.h"

typedef struct {
	size_t head_ptr;
	word_u head;
	word_u buffer[RAS_SIZE];

	enum {
		RAS_NONE,
		RAS_POP,
		RAS_PUSH,
	} cmd;
	word_u arg;
} ras_t;

/* Update our state, with respect to the specified operation. */
void ras_do(const ras_t *curr, ras_t *next);

