#include "bru.h"

#include <assert.h>

#include "config.h"
#include "util.h"

word_u bru_act_target(const bru_t *bru)
{
	const word_u addr_taken = (word_u) { .u = bru->imm.u };
	assert(~addr_taken.u & 1u);
	const word_u addr_not = (word_u) { .u = bru->pc.u + 4u };
	assert(~addr_not.u & 1u);
	word_u exp;

	switch (bru->op) {
	case BRU_OP_JALR_TO_ROB:
	case BRU_OP_JALR_TO_FETCH:
		tracei("[bru] JALR: %u + %u\n", bru->op1.u, bru->imm.u);
		exp = (word_u) {.u = (bru->op1.u + bru->imm.u) & ~1u};
		break;
	case BRU_OP_EQ:
		tracei("[bru] CMP: %u == %u", bru->op1.u, bru->op2.u);
		exp = (bru->op1.u == bru->op2.u) ? addr_taken : addr_not;
		break;
	case BRU_OP_NE:
		tracei("[bru] CMP: %u != %u", bru->op1.u, bru->op2.u);
		exp = (bru->op1.u != bru->op2.u) ? addr_taken : addr_not;
		break;
	case BRU_OP_LT:
		tracei("[bru] CMP: %d < %d", bru->op1.s, bru->op2.s);
		exp = (bru->op1.s < bru->op2.s) ? addr_taken : addr_not;
		break;
	case BRU_OP_GE:
		tracei("[bru] CMP: %d >= %d", bru->op1.s, bru->op2.s);
		exp = (bru->op1.s >= bru->op2.s) ? addr_taken : addr_not;
		break;
	case BRU_OP_LTU:
		tracei("[bru] CMP: %u < %u", bru->op1.u, bru->op2.u);
		exp = (bru->op1.u < bru->op2.u) ? addr_taken : addr_not;
		break;
	case BRU_OP_GEU:
		tracei("[bru] CMP: %u >= %u", bru->op1.u, bru->op2.u);
		exp = (bru->op1.u >= bru->op2.u) ? addr_taken : addr_not;
		break;
	default:
		assert(0);
	}

	if (exp.u != bru->predicted_taddr.u) {
		if (bru->op == BRU_OP_JALR_TO_FETCH || opt_nospec) {
			tracei(" (btac miss)\n");
		} else if (bru->op == BRU_OP_JALR_TO_ROB) {
			tracei(" (mispredict)\n");
		} else if (bru->predicted_taddr.u == addr_taken.u) {
			tracei(" (predicted taken, mispredict)\n");
		} else if (bru->predicted_taddr.u == addr_not.u) {
			tracei(" (predicted not taken, mispredict)\n");
		} else {
			assert(0);
		}
		tracei("[bru] Jmp to %x\n", exp.u);
	} else {
		tracei(" (as predicted)\n");
	}
	return exp;
}

