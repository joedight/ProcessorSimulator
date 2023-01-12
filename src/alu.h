#pragma once

#include "word.h"

/* ALU - Addition, etc. */

enum alu_op {
	/* funct3 | 0x100,
	 * except ADD/SUB, SRL/SRA */
	ALU_OP_ADD	= 0x100,
	ALU_OP_SUB	= 0x110,
	ALU_OP_SLT	= 0x102,
	ALU_OP_SLTU	= 0x103,
	ALU_OP_XOR	= 0x104,
	ALU_OP_OR	= 0x106,
	ALU_OP_AND	= 0x107,
	ALU_OP_SLL	= 0x101,
	ALU_OP_SRL	= 0x105,
	ALU_OP_SRA	= 0x115,

	ALU_OP_SET = 0x100,
};

typedef struct {
	enum alu_op op;
	word_u op1, op2;
	size_t rob_id;
	size_t clk_start;
} alu_t;

enum alu_op instr_alu_op(word_u instr);

/* Return the result of an ALU operation. */
word_u alu_result(const alu_t *alu);

