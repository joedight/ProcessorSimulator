#pragma once
/* Branch unit. */

#include "word.h"

enum bru_op {
	BRU_OP_EQ	= 0x10,
	BRU_OP_NE	= 0x11,
	BRU_OP_LT	= 0x14,
	BRU_OP_GE	= 0x15,
	BRU_OP_LTU	= 0x16,
	BRU_OP_GEU	= 0x17,
	BRU_OP_JALR_TO_FETCH,
	BRU_OP_JALR_TO_ROB,

	BRU_OP_SET = 0x10,
};

typedef struct {
	enum bru_op op;
	word_u op1, op2;
	size_t rob_id;

	word_u pc;
	word_u imm;

	word_u predicted_taddr;
} bru_t;

/* The final/correct taddr from a BRU. */
word_u bru_act_target(const bru_t *bru);

