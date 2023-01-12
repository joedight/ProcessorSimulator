/* Reservation Station.
 * Stores an instruction before it reaches an execution unit. */
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "word.h"

enum rs_type {
	RS_LOAD = 1,
	RS_STORE,
	RS_ALU,
	RS_BR,
	RS_DBG,

	RS_TYPE_COUNT = RS_DBG,
};

const char *rs_type_str(enum rs_type t);

typedef struct {
	/* Fields for sim */
	size_t clk;
	enum rs_type type;

	/* Architectural Fields */
	word_u op;
	size_t qj, qk;
	word_u vj, vk;
	word_u pc;
	word_u immediate;
	word_u addr;
	bool busy;
	word_u predicted_taddr;

	size_t rob_id;
} rs_t;

/* Allocate a reservation station. */
void rs_allocate(rs_t *rs, enum rs_type type, word_u pc, word_u op, size_t clk);

