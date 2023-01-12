#pragma once
/* LSU */
#include "word.h"

enum lsu_op {
	LSU_WIDTH_BYTE = 0,
	LSU_WIDTH_HALF = 1,
	LSU_WIDTH_WORD = 2,

	LSU_WIDTH_MASK = 3,

	LSU_UNSIGNED_BIT = 4,
	LSU_READ_BIT = 8,

	LSU_WRITE_BIT = 16,

	LSU_OP_SB = LSU_WIDTH_BYTE | LSU_WRITE_BIT,
	LSU_OP_SH = LSU_WIDTH_HALF | LSU_WRITE_BIT,
	LSU_OP_SW = LSU_WIDTH_WORD | LSU_WRITE_BIT,

	LSU_OP_LB = LSU_WIDTH_BYTE | LSU_READ_BIT,
	LSU_OP_LH = LSU_WIDTH_HALF | LSU_READ_BIT,
	LSU_OP_LW = LSU_WIDTH_WORD | LSU_READ_BIT,
	LSU_OP_LBU = LSU_WIDTH_BYTE | LSU_UNSIGNED_BIT | LSU_READ_BIT,
	LSU_OP_LHU = LSU_WIDTH_HALF | LSU_UNSIGNED_BIT | LSU_READ_BIT,
};

typedef struct {
	enum lsu_op op;
	word_u addr;
	size_t rob_id;
	size_t clk_start;
	word_u data_in;

	bool data_out_set;
	bool exception;
	word_u data_out;
} lsu_t;

word_u instr_lsu_op(uint32_t opcode, uint32_t funct3);
word_u memory_op(uint8_t *mem, enum lsu_op op, word_u addr, word_u data_in, bool *exception);

