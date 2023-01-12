#pragma once

#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>

#include "../kernel/include/isa.h"

#include "alu.h"
#include "bht.h"
#include "bru.h"
#include "btac.h"
#include "cdb.h"
#include "config.h"
#include "decode.h"
#include "lsu.h"
#include "ras.h"
#include "rob.h"
#include "rs.h"

#include "config.h"
#include "util.h"
#include "stats.h"

enum {
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

typedef struct {
	size_t rob_id;
	word_u dat;
} reg_t;

typedef struct {
	word_u pc;
	word_u instr;
	btac_entry_t btac;
	bht_entry_t bht;
} fetched_instr_t;

struct per_pc_stats {
	size_t btac_correct,
		btac_incorrect,
		bht_correct,
		bht_incorrect,
		static_correct,
		static_incorrect,
		miss;
	enum rob_branch_type type;
	enum rob_type rob_type;

	size_t retire_stall,
		arg_stall,
		ex_stall,
		issued,
		retired;
};

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

	rs_t rss[RS_COUNT];
	rs_t ldb[LDB_SIZE];

	alu_t alus[ALU_COUNT];
	lsu_t lsus[LSU_COUNT];
	bru_t brus[BRU_COUNT];

	struct bht bht;
	struct btac btac;

	size_t global_branch_history;

	ras_t ras;

	rob_t rob[ROB_SIZE];
	/* Head - index of next insertion,
	 * Tail - index of next read.
	 * If head == tail, rob is empty,
	 * if head == tail+1, rob is full */
	size_t rob_head;
	size_t rob_tail;
	size_t ldb_head;
	size_t ldb_tail;

	struct cdb cdb;

	struct stats stats;
} state_t;

#define FOR_INDEX_ROB(state, i) \
	for (size_t i = state->rob_tail; i != state->rob_head; i = (i + 1) & ROB_INDEX_MASK)

#define IS_BRANCH(instr) ( instr_opcode(instr).u == OPC_JAL || instr_opcode(instr).u == OPC_JALR || instr_opcode(instr).u == OPC_BRANCH)


bool is_link_reg(uint8_t reg);

void pipeline_flush(state_t *next);

bool addrs_may_overlap(word_u this, word_u other);

bool rob_earlier_store_overlaps(const state_t *curr, const lsu_t *lsu, word_u *val, bool *set_val, bool *dbg_wait_val);

void rob_alloc_only(const state_t *curr, state_t *next, rob_t *rob, enum rob_type type, word_u pc);

void rs_alloc_only(state_t *next, rs_t *rs, enum rs_type type, word_u pc, word_u op);

void rs_rob_alloc(const state_t *curr, state_t *next, rs_t *rs, rob_t *rob, enum rob_type rob_type, enum rs_type rs_type, word_u pc, word_u op);

void rob_rd(state_t *next, rob_t *rob, uint8_t rd);

rob_t *rob_find_free(const state_t *curr, state_t *next);

rs_t *ldb_find_free(const state_t *curr, state_t *next);

void ldb_alloc(const state_t *curr, state_t *next, rs_t *new_ldb);

const rs_t *rs_waiting_and_free(const state_t *curr, state_t *next, enum rs_type type);

const rs_t *ldb_next_and_free(const state_t *curr, state_t *next);

void rs_find_free(const state_t *curr, state_t *next, const rs_t **rs_curr, rs_t **rs_next);

void rs_set_rsrc1(rs_t *rs, uint8_t rsrc1, state_t *next);

void rs_set_rsrc2(rs_t *rs, uint8_t rsrc2, state_t *next);

void bht_btac_update(const state_t *curr, state_t *next, word_u pc, size_t global_history, bool taken, word_u taddr);

void upd_branch_stats(const rob_t *entry, state_t *next, struct per_pc_stats *per_pc);


