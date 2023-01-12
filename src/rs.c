#include "rs.h"

#include <assert.h>

const char *rs_type_str(enum rs_type t)
{
	switch (t) {
	case RS_LOAD:
		return "load";
	case RS_STORE:
		return "store";
	case RS_ALU:
		return "alu";
	case RS_BR:
		return "branch";
	case RS_DBG:
		return "debug";
	default:
		return "invalid";
	}
}

void rs_allocate(rs_t *rs, enum rs_type type, word_u pc, word_u op, size_t clk)
{
	assert(rs);
	assert(pc.u);
	assert(op.u);
	rs->rob_id = 0;
	rs->busy = 1;
	rs->clk = clk;
	rs->pc = pc;
	rs->op = op;
	rs->type = type;
}


