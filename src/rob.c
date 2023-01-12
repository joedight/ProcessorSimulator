#include "rob.h"

const char *rob_type_str(enum rob_type t)
{
	switch (t) {
	case ROB_INSTR_BRANCH:
		return "branch";
	case ROB_INSTR_STORE:
		return "store";
	case ROB_INSTR_REGISTER:
		return "reg";
	case ROB_INSTR_DEBUG:
		return "debug";
	default:
		return "invalid";
	}
}

void rob_allocate(rob_t *rob, size_t id, enum rob_type type, word_u pc, size_t global_branch_history)
{
	assert(rob);

	rob->id = id;
	rob->type = type;
	rob->pc = pc;
	if (type == ROB_INSTR_BRANCH)
		rob->branch_ctrl.global_history = global_branch_history;
}

void rob_ready(rob_t *rob, word_u val)
{
	assert(rob->id);
	assert(!rob->ready);
	rob->ready = 1;
	switch (rob->type) {
	case ROB_INSTR_REGISTER:
		assert(!rob->data.reg.val.u);
		rob->data.reg.val = val;
		break;
	case ROB_INSTR_BRANCH:
		assert(!rob->data.brt.act.u);
		rob->data.brt.act = val;
		break;
	default:
		assert(0);
	}
}

word_u rob_get_reg_val(const rob_t *rob)
{
	switch (rob->type) {
	case ROB_INSTR_REGISTER:
		return rob->data.reg.val;
	case ROB_INSTR_DEBUG:
		assert(rob->data.debug.opcode.u == DBG_OP_INPUT);
		return rob->data.debug.operand;
	default:
		assert(0);
	}
}

