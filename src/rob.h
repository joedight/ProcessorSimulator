/* An entry in the ROB. */
#pragma once

#include "util.h"
#include "word.h"
#include "lsu.h"
#include <stddef.h>

#include "../kernel/include/isa.h"

enum rob_type {
	ROB_INSTR_BRANCH = 1,
	ROB_INSTR_STORE,
	ROB_INSTR_REGISTER,
	ROB_INSTR_DEBUG,
};

const char *rob_type_str(enum rob_type t);

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

/* Allocate a ROB entry */
void rob_allocate(rob_t *rob, size_t id, enum rob_type type, word_u pc, size_t global_branch_history);

/* Mark a ROB entry valid, with the provided data. */
void rob_ready(rob_t *rob, word_u val);

/* Get the RD value from a ROB entry. */
word_u rob_get_reg_val(const rob_t *rob);

