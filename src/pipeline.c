#include "simulator.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "debugger.h"

/* Defined in Unpriv Spec, 2.5 */
bool is_link_reg(uint8_t reg)
{
	return (reg == REG_RA || reg == REG_T0);
}

void pipeline_flush(state_t *next)
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

	next->ras = (ras_t) { 0 };

	memset(next->rss, 0, sizeof(next->rss));
	memset(next->ldb, 0, sizeof(next->ldb));
	memset(next->alus, 0, sizeof(next->alus));
	memset(next->lsus, 0, sizeof(next->lsus));
	memset(next->brus, 0, sizeof(next->brus));

	memset(next->rob, 0, sizeof(next->rob));
	next->rob_head = next->rob_tail = 0;
	next->ldb_head = next->ldb_tail = 0;
	next->cdb = (struct cdb){ 0 };
}

bool addrs_may_overlap(word_u this, word_u other)
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

#define IS_BRANCH(instr) ( instr_opcode(instr).u == OPC_JAL || instr_opcode(instr).u == OPC_JALR || instr_opcode(instr).u == OPC_BRANCH)


void bht_btac_update(const state_t *curr, state_t *next, word_u pc, size_t global_history, bool taken, word_u taddr)
{
	if (!feature_branch_bht_btac || opt_nospec)
		return;

	uint8_t ctr = bht_update(&curr->bht, &next->bht, pc, global_history, taken);

	/* BTAC entries stored for predicted-taken branches only (Otherwise fetch just carries on anyway) */
	if (taken && ctr > 1) {
		btac_update(&next->btac, pc, taddr);
	} else if (ctr < 2) {
		btac_update(&next->btac, pc, (word_u){ .u = 0 });
	}
}

void upd_branch_stats(const rob_t *entry, state_t *next, struct per_pc_stats *per_pc)
{
	const word_u pred = entry->data.brt.pred;
	const word_u act = entry->data.brt.act;
	struct per_pc_stats tmp;
	if (!per_pc)
		per_pc = &tmp;
	else
		per_pc = &per_pc[entry->pc.u];
	per_pc->type = entry->dbg_branch_info.type;
	switch (entry->dbg_branch_info.type) {
	case ROB_BRANCH_JAL:
		if (entry->dbg_branch_info.pred == ROB_PRED_BTAC) {
			++next->stats.jal_btac_hits;
			++per_pc->btac_correct;
		} else if (entry->dbg_branch_info.pred == ROB_PRED_NONE) {
			++next->stats.jal_btac_miss;
			++per_pc->miss;
		} else {
			assert(0);
		}
		break;
	case ROB_BRANCH_JALR:
		if (entry->dbg_branch_info.pred == ROB_PRED_NONE) {
			++next->stats.jalr_btac_miss;
		} else if (entry->dbg_branch_info.pred == ROB_PRED_RAS) {
			if (pred.u == act.u) {
				++next->stats.jalr_ras_correct;
			} else {
				++next->stats.jalr_ras_incorrect;
			}
		} else if (entry->dbg_branch_info.pred == ROB_PRED_BTAC) {
			if (pred.u == act.u) {
				++next->stats.jalr_btac_correct;
				++per_pc->btac_correct;
			} else {
				++next->stats.jalr_btac_incorrect;
				++per_pc->btac_incorrect;
			}
		} else {
			assert(0);
		}
		break;
	case ROB_BRANCH_CMP:
		if (entry->dbg_branch_info.pred == ROB_PRED_STATIC || entry->dbg_branch_info.pred == ROB_PRED_NONE) {
			if (pred.u == act.u) {
				++next->stats.cmp_static_correct;
				++per_pc->static_correct;
			} else {
				++next->stats.cmp_static_incorrect;
				++per_pc->static_incorrect;
			}
		} else if (entry->dbg_branch_info.pred == ROB_PRED_BHT) {
			if (pred.u == act.u) {
				++next->stats.cmp_bht_correct;
				++per_pc->bht_correct;
			} else {
				++next->stats.cmp_bht_incorrect;
				++per_pc->bht_incorrect;
			}
		} else if (entry->dbg_branch_info.pred == ROB_PRED_BTAC) {
			if (pred.u == act.u) {
				++next->stats.cmp_btac_correct;
				++per_pc->btac_correct;
			} else {
				++next->stats.cmp_btac_incorrect;
				++per_pc->btac_incorrect;
			}
		} else {
			assert(0);
		}
		break;
	default:
		break;
	}
}


