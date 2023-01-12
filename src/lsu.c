#include "lsu.h"

#include <stdio.h>

#include "config.h"
#include "decode.h"
#include "debugger.h"
#include "util.h"

word_u instr_lsu_op(uint32_t opcode, uint32_t funct3)
{
	word_u ret;
	switch (funct3) {
	case 0:
	case 1:
	case 2:
	case 4:
	case 5:
		ret.u = funct3;
		break;
	default:
		assert(0);
	}
	switch (opcode) {
	case OPC_STORE:
		ret.u |= LSU_WRITE_BIT;
		break;
	case OPC_LOAD:
		ret.u |= LSU_READ_BIT;
		break;
	default:
		assert(0);
	}
	return ret;
}

word_u memory_op(uint8_t *mem, enum lsu_op op, word_u addr, word_u data_in, bool *exception)
{
	uint8_t *buff = mem + addr.u;
	word_u out = { .u = 0 };
	addr.u &= ~1u;

	if (addr.u > MEM_SIZE) {
		if (exception) {
			*exception = 1;
			printf("[lsu] Warn invalid mem access, perhaps speculative.\n");
		} else {
			printf("[lsu] Invalid mem access to %x\n", addr.u);
			debugger_pause = 1;
		}
		return out;
	}

	tracei("[lsu] ");

	if (op & LSU_READ_BIT) {
		tracei("read ");
		switch (op & LSU_WIDTH_MASK) {
		case LSU_WIDTH_WORD:
			out.u |= buff[3] << 24;
			out.u |= buff[2] << 16;
			/* fallthrough */
		case LSU_WIDTH_HALF: 
			out.u |= buff[1] << 8;
			/* fallthrough */
		case LSU_WIDTH_BYTE: 
			out.u |= buff[0];
			break;
		default:
			assert(0);
		}
	} else {
		assert(op & LSU_WRITE_BIT);
		tracei("write ");
		switch (op & LSU_WIDTH_MASK) {
		case LSU_WIDTH_WORD:
			buff[3] = data_in.u >> 24;
			buff[2] = data_in.u >> 16;
			/* fallthrough */
		case LSU_WIDTH_HALF: 
			buff[1] = data_in.u >> 8;
			/* fallthrough */
		case LSU_WIDTH_BYTE: 
			buff[0] = data_in.u;
			break;
		default:
			assert(0);
		}
	}
	switch (op & LSU_WIDTH_MASK) {
	case LSU_WIDTH_WORD:
		tracei("word\n");
		break;
	case LSU_WIDTH_HALF:
		tracei("half\n");
		break;
	case LSU_WIDTH_BYTE:
		tracei("byte\n");
		break;
	default:
		assert(0);
	}
	return out;
}


