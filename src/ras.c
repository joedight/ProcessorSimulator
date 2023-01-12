#include "ras.h"

#include <assert.h>

void ras_do(const ras_t *curr, ras_t *next)
{
	for (size_t i = 0; i < RAS_SIZE; i++) {
		next->buffer[i] = curr->buffer[i];
	}

	switch (curr->cmd) {
	case RAS_NONE:
		next->head_ptr = curr->head_ptr;
		next->head = curr->head;
		assert(!curr->arg.u);
		break;
	case RAS_POP:
		assert(!curr->arg.u);
		next->buffer[curr->head_ptr] = (word_u){ 0 };
		next->head_ptr = (curr->head_ptr - 1) & RAS_INDEX_MASK;
		break;
	case RAS_PUSH:
		assert(curr->arg.u);
		next->head_ptr = (curr->head_ptr + 1) & RAS_INDEX_MASK;
		next->buffer[next->head_ptr] = curr->arg;
		break;
	default:
		assert(0);
	}
	next->head = next->buffer[next->head_ptr];
}

