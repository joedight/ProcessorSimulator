#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>

#include "config.h"

static bool tracei_enabled = 1;
void tracei(char *fmt, ...)
{
	if (!tracei_enabled)
		return;

	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

const char *reg_name(uint32_t reg)
{
	switch (reg) {
	case 0: return "zero";
	case 1: return "ra";
	case 2: return "sp";
	case 3: return "gp";
	case 4: return "tp";
	case 5: return "t0";
	case 6: return "t1";
	case 7: return "t2";
	case 8: return "s0/fp";
	case 9: return "s1";
	case 10: return "a0";
	case 11: return "a1";
	case 12: return "a2";
	case 13: return "a3";
	case 14: return "a4";
	case 15: return "a5";
	case 16: return "a6";
	case 17: return "a7";
	case 18: return "s2";
	case 19: return "s3";
	case 20: return "s4";
	case 21: return "s5";
	case 22: return "s6";
	case 23: return "s7";
	case 24: return "s8";
	case 25: return "s9";
	case 26: return "s10";
	case 27: return "s11";
	case 28: return "t3";
	case 29: return "t4";
	case 30: return "t5";
	case 31: return "t6";
	default: return "invalid";
	}
}

typedef union {
	uint32_t u;
	int32_t s;
} word_u;

enum {
	MEM_SIZE = 1024ul * 1024 * 1024 * 2,
	BIN_OFFSET = 4096,

	STACK_LOCATION = MEM_SIZE - 8,
	THREAD_LOCATION = 1024ul * 1024 * 768,

	REG_RA = 1,
	REG_SP = 2,
	REG_GP = 3,
	REG_TP = 4,
	REG_T0 = 5,

	REG_T3 = 28,
	REG_T4 = 29,
};

enum {
	OPC_LUI		= 0x37,
	OPC_AUIPC	= 0x17,
	OPC_JAL		= 0x6F,
	OPC_JALR	= 0x67,
	OPC_BRANCH	= 0x63,
	OPC_LOAD	= 0x03,
	OPC_STORE	= 0x23,
	OPC_REG_IMM	= 0x13,
	OPC_REG_REG	= 0x33,
	OPC_FENCE	= 0x0F,
	OPC_ENV		= 0x73,
};

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

#include "kernel/include/isa.h"

enum rs_type {
	RS_LOAD = 1,
	RS_STORE,
	RS_ALU,
	RS_BR,
	RS_DBG,

	RS_TYPE_COUNT = RS_DBG,
};

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

typedef struct {
	enum alu_op op;
	word_u op1, op2;
	size_t rob_id;
	size_t clk_start;
} alu_t;

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

typedef struct {
	enum bru_op op;
	word_u op1, op2;
	size_t rob_id;

	word_u pc;
	word_u imm;

	word_u predicted_taddr;
} bru_t;

enum rob_type {
	ROB_INSTR_BRANCH = 1,
	ROB_INSTR_STORE,
	ROB_INSTR_REGISTER,
	ROB_INSTR_DEBUG,
};

static const char *rob_type_str(enum rob_type t)
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

static const char *rs_type_str(enum rs_type t)
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

/* Prevent paths from not setting bools, then implicitly reading them as 0. */
typedef struct {
	enum {
		_bOOL_TRUE = 2,
		_bOOL_FALSE,
	} _internal;
} bool_t;

static inline bool b_test(bool_t x)
{
	if (x._internal == _bOOL_TRUE)
		return 1;
	else if (x._internal == _bOOL_FALSE)
		return 0;
	else
		assert(0);
}

static inline bool_t b_set(bool x)
{
	return (bool_t) { ._internal = x ? _bOOL_TRUE : _bOOL_FALSE };
}

static inline bool_t b_not(bool_t x)
{
	if (x._internal == _bOOL_TRUE)
		return (bool_t) { ._internal = _bOOL_FALSE };
	else if (x._internal == _bOOL_FALSE)
		return (bool_t) { ._internal = _bOOL_TRUE };
	else
		return (bool_t) { 0 };
}

typedef struct {
	size_t id;

	// Index in BHT/BTAC
	word_u pc;

	enum rob_type type;
	/* Additional per-instr control signals */
	enum lsu_op store_op;
	struct {
		bool_t consider_prediction; // i.e. should we flush when pred != act?
		bool_t change_bht;
		bool_t pred_taken; // only set where change_bht is.
		size_t global_history;
	} branch_ctrl;

	// For stats.
	struct {
		enum rob_branch_type {
			ROB_BRANCH_INVALID,
			ROB_BRANCH_JALR,
			ROB_BRANCH_JAL,
			ROB_BRANCH_CMP,
		} type;
		enum {
			ROB_PRED_INVALID,
			ROB_PRED_STATIC,
			ROB_PRED_NONE,
			ROB_PRED_BTAC,
			ROB_PRED_BHT,
			ROB_PRED_RAS,
		} pred;
	} dbg_branch_info;
	bool dbg_was_load;

	// Mostly 
	union {
		// rob_instr_reg or rob_instr_store
		struct {
			word_u dest;
			word_u val;
		} reg;
		// rob_instr_branch
		struct {
			word_u pred;
			word_u act;
		} brt;
		// rob_instr_dbg
		struct {
			word_u opcode;
			word_u operand;
		} debug;
	} data;

	bool ready;
	bool exception;
} rob_t;

size_t bht_index(word_u pc, size_t global_history)
{
	if (feature_2level) {
		if (opt_gshare)
			return ((pc.u / 4u) ^ global_history) & BHT_INDEX_MASK;
		else
			return (((pc.u / 4u) << GLOBAL_HISTORY_BITS) | (global_history & GLOBAL_HISTORY_MASK)) & BHT_INDEX_MASK;
	} else {
		return (pc.u / 4u) & BHT_INDEX_MASK;
	}
}

typedef struct {
	size_t rob_id;
	word_u data;
	bool exception;
} cdb_t;

typedef struct {
	size_t rob_id;
	word_u dat;
} reg_t;

typedef struct {
	bool valid;
	uint8_t ctr;

	word_u debug_last_pc;
} bht_entry_t;

typedef struct {
	word_u br_pc;
	word_u taddr;
} btac_entry_t;

typedef struct {
	word_u raddr;
} ras_entry_t;

typedef struct {
	word_u pc;
	word_u instr;
	btac_entry_t btac;
	bht_entry_t bht;
} fetched_instr_t;

typedef struct {
	reg_t arf[REG_COUNT];

	size_t clk;

	/* On retire mispredicted conditional branch or JALR. */
	word_u pc_rob_mispredict;
	word_u pc_exec_bru;
	/* On:
	 * 	BTAC hit with wrong address for JAL or BR,
	 * or 	BTAC miss but BHT hit. */
	word_u pc_decode_predict;

	/* Either from BTAC, or just pc+ISSUE_WIDTH */
	word_u pc_fetch;

	/* If we stall and have to repeat ourselves. */
	word_u pc_last;

	bool fetch_wait_rob_mispredict;
	bool fetch_wait_jalr_bru;

	bool decode_is_clear;
	bool decode_drop_next;

	fetched_instr_t fetch_window[ISSUE_WIDTH];
	fetched_instr_t held_window[ISSUE_WIDTH];
//	bool decode_did_hold;

	rs_t rss[RS_COUNT];
	rs_t ldb[LDB_SIZE];

	alu_t alus[ALU_COUNT];
	lsu_t lsus[LSU_COUNT];
	bru_t brus[BRU_COUNT];

	bht_entry_t bht[BHT_SIZE];
	btac_entry_t btac[BTAC_SIZE];

	size_t ras_head_ptr;
	word_u ras_head;
	ras_entry_t ras[RAS_SIZE];

	size_t global_branch_history;

	enum {
		RAS_NONE,
		RAS_POP,
		RAS_PUSH,
	} ras_cmd;
	word_u ras_arg;

	rob_t rob[ROB_SIZE];
	/* Head - index of next insertion,
	 * Tail - index of next read.
	 * If head == tail, rob is empty,
	 * if head == tail+1, rob is full */
	size_t rob_head;
	size_t rob_tail;
	size_t ldb_head;
	size_t ldb_tail;

	cdb_t cdb[CDB_WIDTH];

	struct stats {
		size_t start_clk,
			issued,
			retired,
			flushed,
			stalled,
			stall_mispredict,

			wait_args,
			wait_ex,
			wait_cdb,
			wait_store_addr,
			wait_store_data,

			recursion_depth,
			recursion_depth_max,

			fetch_window_cnt,
			fetch_window_sum,

			branches,
			loads,
			stores,
			arithmetic,
			env,

			bht_conflicts,

			jal_btac_hits,
			jal_btac_miss,

			jalr_ras_correct,
			jalr_ras_incorrect,
			jalr_btac_correct,
			jalr_btac_incorrect,
			jalr_btac_miss,

			cmp_bht_correct,
			cmp_bht_incorrect,
			cmp_btac_correct,
			cmp_btac_incorrect,
			cmp_static_correct,
			cmp_static_incorrect;
	} stats;
} state_t;

/* Defined in Unpriv Spec, 2.5 */
bool is_link_reg(uint8_t reg)
{
	return (reg == REG_RA || reg == REG_T0);
}

cdb_t *cdb_find_free(state_t *next)
{
	for (size_t i = 0; i < CDB_WIDTH; i++) {
		if (next->cdb[i].rob_id == 0) {
			assert(next->cdb[i].data.u == 0);
			return &next->cdb[i];
		}
	}
	return NULL;
}

const cdb_t *cdb_with_rob(const state_t *curr, size_t rob_id)
{
	assert(rob_id);
	for (size_t i = 0; i < CDB_WIDTH; i++) {
		if (curr->cdb[i].rob_id == rob_id) {
			return &curr->cdb[i];
		}
	}
	return NULL;
}

static void pipeline_flush(state_t *next)
{
	for (size_t i = 0; i < REG_COUNT; i++)
		next->arf[i].rob_id = 0;

	next->pc_last =
	next->pc_fetch =
	next->pc_rob_mispredict =
	next->pc_decode_predict = (word_u){ 0 };
	assert(next->fetch_wait_rob_mispredict);
	next->decode_is_clear = 0;
	next->decode_drop_next = 0;

	memset(next->fetch_window, 0, sizeof(next->fetch_window));
	memset(next->held_window, 0, sizeof(next->held_window));

	next->ras_head_ptr = 0;
	next->ras_head.u = 0;
	next->ras_cmd = RAS_NONE;
	next->ras_arg.u = 0;
	memset(next->ras, 0, sizeof(next->ras));

	memset(next->rss, 0, sizeof(next->rss));
	memset(next->ldb, 0, sizeof(next->ldb));
	memset(next->alus, 0, sizeof(next->alus));
	memset(next->lsus, 0, sizeof(next->lsus));
	memset(next->brus, 0, sizeof(next->brus));

	memset(next->rob, 0, sizeof(next->rob));
	next->rob_head = next->rob_tail = 0;
	next->ldb_head = next->ldb_tail = 0;
	memset(next->cdb, 0, sizeof(next->cdb));
}

inline static bool addrs_may_overlap(word_u this, word_u other)
{
	assert(this.u);
	return (other.u == 0 && !opt_nostorechk)
		|| this.u == other.u
	    	|| this.u == other.u + 1
	       	|| this.u == other.u + 2
	       	|| this.u == other.u + 3
	       	|| this.u + 1 == other.u
	       	|| this.u + 2 == other.u
	       	|| this.u + 3 == other.u;
}

#define FOR_INDEX_ROB(state, i) \
	for (size_t i = state->rob_tail; i != state->rob_head; i = (i + 1) & ROB_INDEX_MASK)


bool rob_earlier_store_overlaps(const state_t *curr, const lsu_t *lsu, word_u *val, bool *set_val, bool *dbg_wait_val)
{
	bool flag = false;
	*set_val = 0;
	val->u = 0;
	FOR_INDEX_ROB(curr, i) {
		if (curr->rob[i].id == lsu->rob_id) {
			return flag;
		}
		if (
			curr->rob[i].type == ROB_INSTR_STORE &&
			addrs_may_overlap(lsu->addr, curr->rob[i].data.reg.dest)
		) {
			flag = 1;
			if (curr->rob[i].data.reg.dest.u == lsu->addr.u &&
				curr->rob[i].store_op & lsu->op & LSU_WIDTH_MASK
			) {
				*dbg_wait_val = 1;
				if (curr->rob[i].ready) {
					*set_val = 1;
					*val = curr->rob[i].data.reg.val;
				} else {
					*set_val = 0;
				}
			} else {
				*dbg_wait_val = 0;
				*set_val = 0;
			}
		}
	}
	assert(0);
}

void rob_alloc_only(const state_t *curr, state_t *next, rob_t *rob, enum rob_type type, word_u pc)
{
	assert(rob && next);
	assert(rob == &next->rob[next->rob_head]);

	rob->id = next->rob_head + 1;
	rob->type = type;
	rob->pc = pc;
	if (type == ROB_INSTR_BRANCH)
		rob->branch_ctrl.global_history = curr->global_branch_history;

	if (next->rob_head == 0) {
		//printf("abcde %lu %lu %s\n", next->rob_head, rob->id, rob_type_str(rob->type));
	}

	next->rob_head = (next->rob_head + 1) & ROB_INDEX_MASK;
	assert(curr->rob_tail != next->rob_head);
}

void rs_alloc_only(state_t *next, rs_t *rs, enum rs_type type, word_u pc, word_u op)
{
	assert(rs);
	assert(pc.u);
	assert(op.u);
	rs->rob_id = 0;
	rs->busy = 1;
	rs->clk = next->clk;
	rs->pc = pc;
	rs->op = op;
	rs->type = type;
}

void rs_rob_alloc(const state_t *curr, state_t *next, rs_t *rs, rob_t *rob, enum rob_type rob_type, enum rs_type rs_type, word_u pc, word_u op)
{
	rob_alloc_only(curr, next, rob, rob_type, pc);
	rs_alloc_only(next, rs, rs_type, pc, op);

	rs->rob_id = rob->id;
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

void rob_rd(state_t *next, rob_t *rob, uint8_t rd)
{
	assert(rd);
	assert(rob);
	assert(rob->id);
	assert(rd && rd < REG_COUNT);

	next->arf[rd].rob_id = rob->id;

	switch (rob->type) {
	case ROB_INSTR_REGISTER:
		rob->data.reg.dest.u = rd;
		break;
	case ROB_INSTR_DEBUG:
		rob->data.debug.operand.u = rd;
		break;
	default:
		assert(0);
	}
}

rob_t *rob_find_free(const state_t *curr, state_t *next)
{
	size_t ni = (next->rob_head + 1) & ROB_INDEX_MASK;
	if (ni == curr->rob_tail) {
		return NULL;
	} else {
		return &next->rob[next->rob_head];
	}
}

rs_t *ldb_find_free(const state_t *curr, state_t *next)
{
	size_t ni = (next->ldb_head + 1) & LDB_INDEX_MASK;
	rs_t *ldb = &next->ldb[next->ldb_head];
	if (ni == curr->ldb_tail || ldb->busy) {
		return NULL;
	} else {
		assert(!ldb->busy);
		return ldb;
	}
}

void ldb_alloc(const state_t *curr, state_t *next, rs_t *new_ldb)
{
	size_t ni = (next->ldb_head + 1) & LDB_INDEX_MASK;
	rs_t *ldb = &next->ldb[next->ldb_head];
	assert(ldb == new_ldb);
	assert(!ldb->busy);
	assert(ni != curr->ldb_tail);
	next->ldb_head = ni;
}

const rs_t *rs_waiting_and_free(const state_t *curr, state_t *next, enum rs_type type)
{
	const rs_t *best = NULL;
	rs_t *best_next = NULL;
	for (size_t i = 0; i < RS_COUNT; i++) {
		if (
			curr->rss[i].busy && curr->rss[i].qj == 0
			&& curr->rss[i].qk == 0
			&& curr->rss[i].type == type
		       	&& next->rss[i].busy
			&& (!best || curr->rss[i].clk < best->clk)
		) {
			assert(type == RS_BR || curr->rss[i].rob_id);
			best = &curr->rss[i];
			best_next = &next->rss[i];
		}
	}
	if (best) {
		*best_next = (rs_t) { 0 };
	}
	return best;
}

const rs_t *ldb_next_and_free(const state_t *curr, state_t *next)
{
	rs_t *new_tail = &next->ldb[next->ldb_tail];
	const rs_t *curr_tail = &curr->ldb[next->ldb_tail];
	if (
		curr->ldb_head == next->ldb_tail
		|| curr_tail->qj || curr_tail->qk
	) {
		return NULL;
	} else {
		assert(curr_tail->busy);
		if (!new_tail->busy)
			return NULL;
		*new_tail = (rs_t) { 0 };
		next->ldb_tail = (next->ldb_tail + 1) & LDB_INDEX_MASK;
		return curr_tail;
	}
}

void rs_find_free(const state_t *curr, state_t *next, const rs_t **rs_curr, rs_t **rs_next)
{
	for (size_t i = 0; i < RS_COUNT; i++) {
		if (!curr->rss[i].busy && !next->rss[i].busy) {
			*rs_curr = &curr->rss[i];
			*rs_next = &next->rss[i];
			return;
		}
	}
	*rs_curr = *rs_next = NULL;
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

void rs_set_rsrc1(rs_t *rs, uint8_t rsrc1, state_t *next)
{
	if (rsrc1 == 0) {
		rs->vj.u = 0;
		rs->qj = 0;
		return;
	}

	const reg_t *reg = &next->arf[rsrc1];
	if (reg->rob_id) {
		const rob_t *rob = &next->rob[reg->rob_id - 1];
		assert(rob->type == ROB_INSTR_REGISTER ||
			rob->type == ROB_INSTR_DEBUG);
		if (rob->ready) {
			rs->vj = rob_get_reg_val(rob);
			rs->qj = 0;
		} else {
			rs->vj.u = 0xABABABAB;
			rs->qj = reg->rob_id;
		}
	} else {
		rs->vj = next->arf[rsrc1].dat;
		rs->qj = 0;
	}
}

void rs_set_rsrc2(rs_t *rs, uint8_t rsrc2, state_t *next)
{
	if (rsrc2 == 0) {
		rs->vk.u = 0;
		rs->qk = 0;
		return;
	}

	const reg_t *reg = &next->arf[rsrc2];
	if (reg->rob_id) {
		const rob_t *rob = &next->rob[reg->rob_id - 1];
		assert(rob->type == ROB_INSTR_REGISTER ||
			rob->type == ROB_INSTR_DEBUG);
		if (rob->ready) {
			rs->vk = rob_get_reg_val(rob);
			rs->qk = 0;
		} else {
			rs->vk.u = 0xABABABAB;
			rs->qk = reg->rob_id;
		}
	} else {
		rs->vk = next->arf[rsrc2].dat;
		rs->qk = 0;
	}
}

word_u lsu_do_op(uint8_t *mem, enum lsu_op op, word_u addr, word_u data_in, bool *exception);

/* Instr decoding */
static inline word_u instr_opcode(word_u i)
{
	i.u = i.u & 0x7F;
	return i;
}

static inline word_u instr_funct3(word_u i)
{
	i.u = (i.u >> 12) & 0x7;
	return i;
}

static inline word_u instr_funct7(word_u i)
{
	i.u = (i.u >> 25) & 0x7F;
	return i;
}

static inline uint8_t instr_rs1(word_u i)
{
	return (i.u >> 15) & 0x1F;
}

static inline uint8_t instr_rs2(word_u i)
{
	return (i.u >> 20) & 0x1F;
}

static inline uint8_t instr_rd(word_u i)
{
	return (i.u >> 7) & 0x1F;
}

static inline word_u instr_imm_itype(word_u i)
{
	i.s >>= 7 + 5 + 3 + 5;
	return i;
}

static inline word_u instr_imm_stype(word_u i)
{
	word_u out;
	out.u = (i.u >> 7) & 0x1F;

	i.s >>= 7 + 3 + 5 + 5;
	out.u |= i.u & ~0x1F;

	return out;
}

static inline word_u instr_imm_btype(word_u i)
{
	word_u out;

	i.u >>= 7;

	out.u = (i.u & 1) << 11;
	out.u |= i.u & 0x1E;
	
	i.u >>= 5 + 3 + 5;

	out.u |= i.u & 0x7E0;

	if (i.u & 0b100000000000)
		out.u |= ~ 0b11111111111;

	return out;
}

static inline word_u instr_imm_utype(word_u i)
{
	i.u = i.u & ~0xFFF;
	return i;
}

static inline word_u instr_imm_jtype(word_u i)
{
	word_u out;
	//            109876543210
	out.u = i.u &  0b10000000000000000000000000000000;
	//             00000000000100000000000000000000
	out.s >>= 11;
	out.u |= i.u & 0b00000000000011111111000000000000;
	//             00000000000111111111000000000000
	
	out.u |=(i.u & 0b00000000000100000000000000000000) >> 9;

	out.u |=(i.u & 0b01111111111000000000000000000000) >> 20;

	return out;
}

#define IS_BRANCH(instr) ( instr_opcode(instr).u == OPC_JAL || instr_opcode(instr).u == OPC_JALR || instr_opcode(instr).u == OPC_BRANCH)

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

