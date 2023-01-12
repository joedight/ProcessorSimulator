#include "alu.h"

#include <stdarg.h>

#include "decode.h"
#include "util.h"

enum alu_op instr_alu_op(word_u instr)
{
	const uint32_t opcode = instr_opcode(instr).u;
	const uint32_t funct3 = instr_funct3(instr).u;
	const uint32_t funct7 = instr_funct7(instr).u;

	switch (opcode) {
	case OPC_REG_IMM:
		switch (funct3 | ALU_OP_SET) {
		case ALU_OP_SRL:
			switch (funct7) {
			case 0x0:
				return ALU_OP_SRL;
			case 0x20:
				return ALU_OP_SRA;
			default:
				assert(0);
			}
		case ALU_OP_SLL:
			assert(funct7 == 0);
			/* fallthrough */
		default:
			return ALU_OP_SET | funct3;
		}
		assert(0);
	case OPC_REG_REG:
		switch (funct3 | ALU_OP_SET) {
		case ALU_OP_ADD:
			switch (funct7) {
			case 0x0:
				return ALU_OP_ADD;
			case 0x20:
				return ALU_OP_SUB;
			default:
				assert(0);
			}
			break;
		case ALU_OP_SRL:
			switch (funct7) {
			case 0x0:
				return ALU_OP_SRL;
			case 0x20:
				return ALU_OP_SRA;
			default:
				assert(0);
			}
			break;
		default:
			assert(funct7 == 0);
			return ALU_OP_SET | funct3;
		}
		/* fallthrough */
	default:
		assert(0);
	}
}

word_u alu_result(const alu_t *alu)
{
	switch (alu->op) {
	case ALU_OP_ADD:
		tracei("[ex] ALU: %u + %u (%d + %d)\n", alu->op1.u, alu->op2.u, alu->op1.s, alu->op2.s);
		return (word_u) { .u = alu->op1.u + alu->op2.u };
	case ALU_OP_SUB:
		tracei("[ex] ALU: %d - %d\n", alu->op1.s, alu->op2.s);
		return (word_u) { .u = alu->op1.u - alu->op2.u };
	case ALU_OP_SLT:
		tracei("[ex] ALU: %d < %d\n", alu->op1.s, alu->op2.s);
		return (word_u) { .u = alu->op1.s < alu->op2.s };
	case ALU_OP_SLTU:
		tracei("[ex] ALU: %u < %u\n", alu->op1.u, alu->op2.u);
		return (word_u) { .u = alu->op1.u < alu->op2.u };
	case ALU_OP_XOR:
		tracei("[ex] ALU: %u ^ %u\n", alu->op1.u, alu->op2.u);
		return (word_u) { .u = alu->op1.u ^ alu->op2.u };
	case ALU_OP_OR:
		tracei("[ex] ALU: %u | %u\n", alu->op1.u, alu->op2.u);
		return (word_u) { .u = alu->op1.u | alu->op2.u };
	case ALU_OP_AND:
		tracei("[ex] ALU: %u & %u\n", alu->op1.u, alu->op2.u);
		return (word_u) { .u = alu->op1.u & alu->op2.u };
	case ALU_OP_SLL:
		tracei("[ex] ALU: %u << %u\n", alu->op1.u, alu->op2.u);
		return (word_u) { .u = alu->op1.u << (alu->op2.u & 0x1F) };
	case ALU_OP_SRL:
		tracei("[ex] ALU: %u >> %u (logical)\n", alu->op1.u, alu->op2.u);
		return (word_u) { .u = alu->op1.u >> (alu->op2.u & 0x1F) };
	case ALU_OP_SRA:
		tracei("[ex] ALU: %d >> %u (arithmetic)\n", alu->op1.s, alu->op2.u);
		return (word_u) { .s = alu->op1.s >> (alu->op2.u & 0x1F) };
	default:
		assert(0);
	}
}


