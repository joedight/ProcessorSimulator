#include "simulator.h"

#include <signal.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t debugger_pause = 1;

word_u lsu_do_op(uint8_t *mem, enum lsu_op op, word_u addr, word_u data_in, bool *exception)
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

word_u alu_do_op(enum alu_op op, word_u op1, word_u op2)
{
	switch (op) {
	case ALU_OP_ADD:
		tracei("[ex] ALU: %u + %u (%d + %d)\n", op1.u, op2.u, op1.s, op2.s);
		return (word_u) { .u = op1.u + op2.u };
	case ALU_OP_SUB:
		tracei("[ex] ALU: %d - %d\n", op1.s, op2.s);
		return (word_u) { .u = op1.u - op2.u };
	case ALU_OP_SLT:
		tracei("[ex] ALU: %d < %d\n", op1.s, op2.s);
		return (word_u) { .u = op1.s < op2.s };
	case ALU_OP_SLTU:
		tracei("[ex] ALU: %u < %u\n", op1.u, op2.u);
		return (word_u) { .u = op1.u < op2.u };
	case ALU_OP_XOR:
		tracei("[ex] ALU: %u ^ %u\n", op1.u, op2.u);
		return (word_u) { .u = op1.u ^ op2.u };
	case ALU_OP_OR:
		tracei("[ex] ALU: %u | %u\n", op1.u, op2.u);
		return (word_u) { .u = op1.u | op2.u };
	case ALU_OP_AND:
		tracei("[ex] ALU: %u & %u\n", op1.u, op2.u);
		return (word_u) { .u = op1.u & op2.u };
	case ALU_OP_SLL:
		op2.u &= 0x1F;
		tracei("[ex] ALU: %u << %u\n", op1.u, op2.u);
		return (word_u) { .u = op1.u << op2.u };
	case ALU_OP_SRL:
		op2.u &= 0x1F;
		tracei("[ex] ALU: %u >> %u (logical)\n", op1.u, op2.u);
		return (word_u) { .u = op1.u >> op2.u };
	case ALU_OP_SRA:
		op2.u &= 0x1F;
		tracei("[ex] ALU: %d >> %u (arithmetic)\n", op1.s, op2.u);
		return (word_u) { .s = op1.s >> op2.u };
	default:
		assert(0);
	}
}

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

void btac_update(state_t *next, word_u pc, word_u taddr)
{
	if (opt_nospec)
		return;

	if (taddr.u) {
		next->btac[(pc.u / 4) & BTAC_INDEX_MASK].br_pc = pc;
	} else {
		next->btac[(pc.u / 4) & BTAC_INDEX_MASK].br_pc = (word_u) { .u = 0 };
	}
	next->btac[(pc.u / 4) & BTAC_INDEX_MASK].taddr = taddr;
}

void bht_btac_update(const state_t *curr, state_t *next, word_u pc, size_t global_history, bool taken, word_u taddr)
{
	if (!feature_branch_bht_btac || opt_nospec)
		return;
	size_t index = bht_index(pc, global_history);
	const bht_entry_t *old = &curr->bht[index];
	bht_entry_t *e = &next->bht[index];
	if (old->valid) {
		assert(old->debug_last_pc.u);
		if (old->debug_last_pc.u != pc.u) {
			tracei("[BHT] conflict: %x overwrites %x\n", pc.u, old->debug_last_pc.u);
//			assert(bht_index(old->debug_last_pc, 0) == bht_index(pc, 0));
			next->stats.bht_conflicts++;
		}

		uint8_t new_ctr;
		if (opt_1bitbht) {
			new_ctr = taken ? 3 : 0;
		} else {
			if (taken && old->ctr != 3) {
				new_ctr = old->ctr + 1;
			} else if (!taken && old->ctr != 0) {
				new_ctr = old->ctr - 1;
			} else {
				new_ctr = old->ctr;
			}
		}
		tracei("[BHT] %x %staken: %d -> %d\n", pc.u, taken ? "" : "not ", e->ctr, new_ctr);
		e->ctr = new_ctr;
	} else {
		e->valid = 1;
		e->ctr = taken ? 2 : 1;
		tracei("[BHT] %x %staken, initialised to %d\n", pc.u, taken ? "" : "not ", e->ctr);
	}
	e->debug_last_pc = pc;

	/* BTAC entries stored for predicted-taken branches only (Otherwise fetch just carries on anyway) */
	if (taken && e->ctr > 1)
		btac_update(next, pc, taddr);
	else if (e->ctr < 2)
		btac_update(next, pc, (word_u){ .u = 0 });
}

void handle_sigint(int _)
{
	if (debugger_pause)
		_Exit(0);
	debugger_pause = 1;
}

size_t binary_load(char *flName, uint8_t *mem, word_u *entrypoint)
{
	FILE *f  = fopen(flName, "rb");
	if (!f) {
		fprintf(stderr, "Couldn't open file.\n");
		return 0;
	}

	fseek(f, 0L, SEEK_END);
	size_t x = ftell(f);
	rewind(f);

	assert(x < MEM_SIZE && "Binary is too big.");

	fread(mem + BIN_OFFSET, sizeof(uint8_t), x, f);
	fclose(f);

	printf("Have binary of size %lu.\n", x);

	int l = strlen(flName);
	flName[l-3] = 'e';
	flName[l-2] = 'n';
	flName[l-1] = 'p';

	f = fopen(flName, "rb");
	if (!f) {
		fprintf(stderr, "Couldn't open .enp file.\n");
		return 0;
	}
	if (fscanf(f, "%x", &entrypoint->u) != 1) {
		fprintf(stderr, "Couldn't read entry point.\n");
		return 0;
	}
	entrypoint->u += BIN_OFFSET;
	return x;
}

struct breakpoint {
	word_u addr;
//	struct list_head list;
};

void debugger_print(const state_t *next, const char *arg)
{
	if (strcmp(arg, "rob") == 0) {
		printf("tail: %lu, head: %lu\n", next->rob_tail, next->rob_head);
		printf("id\tpc\ttype\t\tready\tval\tdest\n");
		FOR_INDEX_ROB(next, i) {
			const rob_t *rob = &next->rob[i];
			printf("%lu\t%x\t%s\t\t %d\t",
				rob->id,
				rob->pc.u,
				rob_type_str(rob->type),
				rob->ready
			);
			switch (rob->type) {
			case ROB_INSTR_REGISTER:
				printf("%x\t%s\n", rob->data.reg.val.u,
					reg_name(rob->data.reg.val.u));
			break; case ROB_INSTR_STORE:
				printf("%x\t%x\n", rob->data.reg.val.u,
					rob->data.reg.val.u);
			break; case ROB_INSTR_BRANCH:
				printf("%x\t%x\n", rob->data.brt.pred.u,
					rob->data.brt.act.u);
			break; default:
				printf("\n");
			}
		}
	} else if (strcmp(arg, "reg") == 0) {
		for (size_t i = 0; i < REG_COUNT; i++) {
			if ((i & 3) == 0)
				printf("\n");
			printf("%5s: %.8x (rs %lu)\t", reg_name(i), next->arf[i].dat.u, next->arf[i].rob_id);
		}
		printf("\n");
	} else if (strcmp(arg, "rs") == 0) {
		printf("Ldb tail: %lu, head: %lu\n", next->ldb_tail, next->ldb_head);
		printf("\tpc\ttype\tvk\tqk\t\tvj\tqj\t\n");
		for (size_t i = 0; i < RS_COUNT + LDB_SIZE; i++) {
			const rs_t *rs;
			if (i < RS_COUNT)
				rs = &next->rss[i];
			else
				rs = &next->ldb[i - RS_COUNT];
			if (rs->busy) {
				printf("%lu\t%x\t%s\t%lu\t%x\t\t%lu\t%x\n",
					rs->rob_id, 
					rs->pc.u,
					rs_type_str(rs->type),
					rs->qj, rs->vj.u,
					rs->qk, rs->vk.u);
			}
		}
	} else if (strcmp(arg, "bht") == 0) {
		for (size_t i = 0; i < BHT_SIZE; i++) {
			if (next->bht[i].valid) {
				printf("%x: %d\n", next->bht[i].debug_last_pc.u, next->bht[i].ctr);
			}
		}
	} else if (strcmp(arg, "ras") == 0) {
		printf("Head: %lu, %x\n", next->ras_head_ptr, next->ras_head.u);
		for (size_t i = next->ras_head_ptr; ((i - 1) & RAS_INDEX_MASK) != next->ras_head_ptr; i = (i - 1) & RAS_INDEX_MASK) {
			printf("%lu - %x\n", i, next->ras[i].raddr.u);
		}
	} else {
		printf("Unknown thing: %s\n", arg);
	}
}

int debugger(state_t *next, const uint8_t *mem /*, struct list_head breakpoints */)
{
	if (debugger_pause) {
		printf("PC: %x ish\n", next->pc_last.u);
	}
	while (debugger_pause) {
		fputs("\n>", stdout);
		fflush(stdout);
		char cmd = '\0';
		char arg[32] = { '\0' };
		char line[32] = { '\0' };
		fgets(line, 32, stdin);
		sscanf(line, "%c %31s", &cmd, arg);

		word_u arg_addr;
		bool arg_addr_v = sscanf(arg, "%x", &arg_addr.u);

		switch (cmd) {
		case 'n':
		case '\0':
		case '\n':
			return 0;
		case 'c':
			debugger_pause = 0;
			return 0;
		case 'p':
			debugger_print(next, arg);
			break;
		case 'm':
			if (arg_addr_v) {
				printf("%s", &mem[arg_addr.u]);
			} else {
				printf("Invalid address: %s", arg);
			}
			break;
		case 'b':
			if (arg_addr_v) {
				/*
				struct breakpoint *bp = NULL;
				bool flag = 0;
				list_for_each_entry(bp, &breakpoints, list)
				{
					if (bp->addr.u == arg_addr.u) {
						printf("Unset breakpoint %x\n", arg_addr.u);
						list_del(&bp->list);
						free(bp);
						flag = 1;
						break;
					}
				}

				if (!flag) {
					printf("Set breakpoint %x\n", arg_addr.u);
					bp = calloc(1, sizeof(struct breakpoint));
					bp->addr = arg_addr;
					list_add(&bp->list, &breakpoints);
				}
				*/
			} else {
				printf("Invalid breakpoint\n");
			}

			break;
		case 's':
			tracei_enabled = !tracei_enabled;
			printf(tracei_enabled ? "Debug spew on\n" : "Debug spew off\n");
			break;
		case 'q':
			return 1;
		}
	}
	return 0;
}

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

void ras_do(const state_t *curr, state_t *next)
{
	for (size_t i = 0; i < RAS_SIZE; i++) {
		next->ras[i] = curr->ras[i];
	}

	switch (curr->ras_cmd) {
	case RAS_NONE:
		next->ras_head_ptr = curr->ras_head_ptr;
		next->ras_head = curr->ras_head;
		assert(!curr->ras_arg.u);
		break;
	case RAS_POP:
		next->stats.recursion_depth--;
		assert(!curr->ras_arg.u);
		next->ras[curr->ras_head_ptr] = (ras_entry_t){ 0 };
		next->ras_head_ptr = (curr->ras_head_ptr - 1) & RAS_INDEX_MASK;
		break;
	case RAS_PUSH:
		next->stats.recursion_depth++;
		if (next->stats.recursion_depth > next->stats.recursion_depth_max) {
			next->stats.recursion_depth_max = next->stats.recursion_depth;
		}
		assert(curr->ras_arg.u);
		next->ras_head_ptr = (curr->ras_head_ptr + 1) & RAS_INDEX_MASK;
		next->ras[next->ras_head_ptr].raddr = curr->ras_arg;
		break;
	default:
		assert(0);
	}
	next->ras_head = next->ras[next->ras_head_ptr].raddr;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Specify binary\n");
		return -1;
	}

	/* Debugger sigint handler. */
	{
		struct sigaction sigint;
		sigint.sa_handler = handle_sigint;
		sigemptyset(&sigint.sa_mask);
		sigint.sa_flags = 0;

		sigaction(SIGINT, &sigint, NULL);
	}

	static_assert(sizeof(uint8_t) == 1, "Need 8 bit chars.");

	uint8_t *mem = calloc(1, MEM_SIZE);
	assert(mem);

	word_u entry;
	const size_t bin_size = binary_load(argv[1], mem, &entry);
	const size_t bin_region = BIN_OFFSET + bin_size;
	if (!bin_size) {
		free(mem);
		return -1;
	}

	bool bench_only = 0;
	bool granular_stats = 0;
	bool permissive = 0;
	FILE *trace_pc = NULL;
	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "bench") == 0) {
			bench_only = 1;
			tracei_enabled = 0;
			debugger_pause = 0;
		} else if (strcmp(argv[i], "loud") == 0) {
			tracei_enabled = 1;
		} else if (strcmp(argv[i], "granular") == 0) {
			granular_stats = 1;
		} else if (strcmp(argv[i], "trace") == 0) {
			if (!trace_pc)
				trace_pc = fopen("pc_trace", "wb");
			if (!trace_pc)
				fprintf(stderr, "Failed to open trace file");
		} else if (strcmp(argv[i], "static") == 0) {
			feature_branch_bht_btac = false;
		} else if (strcmp(argv[i], "no2level") == 0) {
			feature_2level = false;
		} else if (strcmp(argv[i], "noforward") == 0) {
			feature_store_forward = false;
		} else if (strcmp(argv[i], "clearhistoryoncall") == 0) {
			opt_clearhistoncall = true;
		} else if (strcmp(argv[i], "1bitbht") == 0) {
			opt_1bitbht = true;
		} else if (strcmp(argv[i], "nospec") == 0) {
			opt_nospec = true;
		} else if (strcmp(argv[i], "gshare") == 0) {
			opt_gshare = true;
		} else if (strcmp(argv[i], "nostorechk") == 0) {
			opt_nostorechk = true;
		} else if (strcmp(argv[i], "permissive") == 0) {
			permissive = true;
		} else {
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			return -1;
		}
	}

//	LIST_HEAD(breakpoints);

	const state_t *curr = NULL;
	state_t *next = calloc(1, sizeof(state_t));

	next->fetch_wait_rob_mispredict = 1;
	next->pc_rob_mispredict = entry;

//	assert(bin_size % 4 == 0);
	struct per_pc_stats *per_pc_stats = NULL;
	if (granular_stats) {
		per_pc_stats = calloc(bin_size, sizeof(struct per_pc_stats));
		if (!per_pc_stats) {
			fprintf(stderr, "Failed to alloc per-instr stats.\n");
		}
	}

	next->arf[REG_SP].dat.u = STACK_LOCATION;
	next->arf[REG_TP].dat.u = THREAD_LOCATION;

	if (debugger_pause)
		printf("Press 'c' to begin execution.\n");

	bool run = 1;
	while (run && !debugger(next, mem /*, breakpoints*/)) {
		if (curr) {
			free((void*)curr);
		}

		curr = next;
		next = calloc(1, sizeof(state_t));
		next->clk = curr->clk + 1;
		next->rob_head = curr->rob_head;
		next->ldb_head = curr->ldb_head;
		next->stats = curr->stats;

		tracei("\n");

		/* Copy arf as-is (we should modify it only in retire) */
		memcpy(next->arf, curr->arf, sizeof(curr->arf));
		/* Same for ROB (ish) */
		memcpy(next->rob, curr->rob, sizeof(curr->rob));
		/* Copy reservation stations, modifying if we're waiting for an operand on the CDB */
		for (size_t i = 0; i < RS_COUNT + LDB_SIZE; i++) {
			const rs_t *old; 
			if (i < RS_COUNT) {
				old = &curr->rss[i];
				assert(old->type != RS_LOAD);
			} else {
				old = &curr->ldb[i - RS_COUNT];
				assert(!old->busy || old->type == RS_LOAD);
			}

			if (old->busy) {
				assert(old->type);
				rs_t *new;
				if (i < RS_COUNT)
					new = &next->rss[i];
				else
					new = &next->ldb[i - RS_COUNT];
				*new = *old;
				const cdb_t *cdb = NULL;
				if (old->qj && (cdb = cdb_with_rob(curr, old->qj))) {
					tracei("[rs] writeback op1 to %lu from %lu\n", old->rob_id, old->qj);
					assert(old->busy);
					new->qj = 0;
					new->vj = cdb->data;
					/* Calc addr and null check for load or store buffer */
					if (old->type == RS_LOAD || old->type == RS_STORE) {
						assert(old->addr.u == 0);
						new->addr.u = old->immediate.u + new->vj.u;
						tracei("[rs] %lu, pc %x now has mem addr %x\n", old->rob_id, old->pc.u, new->addr.u);
						if (!new->addr.u)
							new->addr.u = 0xFFffFFff;
					}
				}
				if (old->qk && (cdb = cdb_with_rob(curr, old->qk))) {
					tracei("[rs] writeback op2 to %lu from %lu\n", old->rob_id, old->qk);
					assert(old->busy);
					new->qk = 0;
					new->vk = cdb->data;
				}
				assert(!cdb_with_rob(curr, old->rob_id));
				if (0 == new->qj && 0 == new->qk) {
					tracei("[rs] %lu ready for ex unit\n", old->rob_id);
					next->stats.wait_ex++;
					if (per_pc_stats)
						per_pc_stats[old->pc.u].ex_stall++;
				} else {
					tracei("[rs] %lu waiting on result from %lu and %lu\n", old->rob_id, old->qj, old->qk);
					next->stats.wait_args++;
					if (per_pc_stats)
						per_pc_stats[old->pc.u].arg_stall++;
				}
			}
		}
		/* Copy ROB, marking ready where appropriate. */
		FOR_INDEX_ROB(curr, i) {
			const rob_t *old = &curr->rob[i];
			rob_t *new = &next->rob[i];
			if (!(old->id && old->type)) {
				fprintf(stderr, "Head: %lu, tail %lu\n", curr->rob_tail, curr->rob_head);
				fprintf(stderr, "Rob entry %lu had id %lu, type %s\n",
					i, old->id, rob_type_str(old->type));
				assert(0);
			}
			assert(old->id);
			assert(old->type);
			assert(old->type != ROB_INSTR_REGISTER ||
				(old->data.reg.dest.u && curr->arf[old->data.reg.dest.u].rob_id));
			*new = *old;

			const cdb_t *cdb = cdb_with_rob(curr, old->id);
			if (cdb) {
				if (old->ready) {
					printf("rob entry %lu ready: %d but had result on cdb\n",
						old->id, old->ready);
					assert(0);
				}
				// Obviously no switching in hw.
				switch (old->type) {
				case ROB_INSTR_REGISTER:
					tracei("[rob] %lu to reg %s have val %u (0x%x)\n",
						old->id,
						reg_name(old->data.reg.dest.u),
						cdb->data.u, cdb->data.u
					);
					new->data.reg.val = cdb->data;
					break;
				case ROB_INSTR_STORE:
					tracei("[rob] %lu store to %x has val %u 0x%x\n",
						old->id,
						old->data.reg.dest.u,
						cdb->data.u, cdb->data.u
					);
					new->data.reg.val = cdb->data;
					break;
				case ROB_INSTR_BRANCH:
					tracei("[rob] %lu have branch target %lu (predicted %lu)\n",
						old->id,
						cdb->data.u,
						old->data.brt.pred
					);
					new->data.brt.act = cdb->data;
					break;
				case ROB_INSTR_DEBUG:
				default:
					assert(0);
				}
				new->ready = 1;
			}
		}

		next->global_branch_history = curr->global_branch_history;

/* Fetch. i.e. take PC from deepest in pipeline, otherwise as PC+4 if allowed. */
		word_u window_pc = { 0 };
		/* for 1-3, decode will be reset anyway. */
		/* 1) Mispredict on rob retire. CMP */
		if (curr->fetch_wait_rob_mispredict) {
			if (curr->pc_rob_mispredict.u) {
				window_pc = curr->pc_rob_mispredict;
				tracei("[if] Mispredict from ROB: pc now %x\n", window_pc);
			} else {
				tracei("[if] Hold on ROB mispredict addr\n");
				next->fetch_wait_rob_mispredict = 1;
				next->stats.stall_mispredict++;
			}
		}
		/* 2) Exec. i.e. JALR where where BTAC missed.
		 * Safe as must have been last issued instr. */
		else if (curr->fetch_wait_jalr_bru) {
			if (curr->pc_exec_bru.u) {
				window_pc = curr->pc_exec_bru;
				tracei("[if] Have JALR addr from BRU: pc now %x\n", window_pc.u);
			} else {
				tracei("[if] Hold on JALR from BRU\n");
				next->fetch_wait_jalr_bru = 1;
			}
		}
		/* 3) Decode (bht/static) branch prediction, or pc+imm jump. JAL, CMP */
		else if (curr->pc_decode_predict.u) {
			window_pc = curr->pc_decode_predict;
			tracei("[if] pc from decode: pc now %x\n", window_pc.u);
		}
		else if (curr->decode_is_clear) {
			/* 4) Fetch (btac) branch prediction (JAL, JALR, CMP), or pc+4 */
			window_pc = curr->pc_fetch;
			tracei("[if] pc from fetch %x\n", window_pc.u);
		}
		else {
			/* If we decode hasn't handled last time's instructions, just send the same again. */
			window_pc = curr->pc_last;
			tracei("[if] pc wait for decode congestion %x\n", window_pc.u);
		}
		if (window_pc.u) {
			next->pc_last = window_pc;
			next->pc_fetch = (word_u) { .u = window_pc.u + ISSUE_WIDTH * 4 };
			size_t i;
			for (i = 0; i < ISSUE_WIDTH; i++) {
				const word_u pc = (word_u) { .u = window_pc.u + i * 4 };
				bool exception = false;
				next->fetch_window[i] = (fetched_instr_t) {
					.pc = pc,
					.instr = lsu_do_op(mem, LSU_OP_LW, pc, (word_u){ .u = 0 }, &exception),
					.btac = curr->btac[(pc.u / 4) & BTAC_INDEX_MASK],
					.bht = curr->bht[bht_index(pc, curr->global_branch_history)],
				};
				if (exception) {
					printf("[warn] Exception on fetch, hope we're speculating. Stalling.\n");
					next->pc_fetch.u = 0u;
					break;
				}
				// In HW we just calculate first BTAC hit in next cycle, and ignore later instrs in decode.
				// Here, break to make debugging easier.
				if (next->fetch_window[i].btac.br_pc.u == pc.u) {
					next->pc_fetch = next->fetch_window[i].btac.taddr;
					break;
				}
			}
			next->stats.fetch_window_cnt++;
			next->stats.fetch_window_sum += i;
		}

		/* Decode/issue. 
		 * Mostly: find free RS and ROB, occupies them.
		 * Also branch prediction / jump handling. */
		fetched_instr_t decode_window[ISSUE_WIDTH] = { 0 };
		next->decode_is_clear = 1;
		if (curr->decode_drop_next) {
			tracei("[id] Got signal to drop next.\n");
		} else if (curr->pc_rob_mispredict.u) {
			tracei("[id] ROB mispredict, id drop fetched\n");
		} else if (curr->pc_decode_predict.u) {
			tracei("[id] BTAC miss but branch predicted, id drop fetched.\n");
//			assert(!curr->id_hold.instr.u);
		} else if (!curr->decode_is_clear) {
			tracei("[id] using held instr(s)\n");
			for (size_t i = 0; i < ISSUE_WIDTH; i++) {
				decode_window[i] = curr->held_window[i];
			}
		} else {
			for (size_t i = 0; i < ISSUE_WIDTH; i++) {
				decode_window[i] = curr->fetch_window[i];
			}
		}

		next->ldb_head = curr->ldb_head;
		for (size_t i = 0; i < ISSUE_WIDTH; i++) {
			const fetched_instr_t instr = decode_window[i];

			tracei("[id] pc %x have instr %x ", instr.pc.u, instr.instr.u);
			const uint32_t opcode = instr_opcode(instr.instr).u;
			const uint32_t funct3 = instr_funct3(instr.instr).u;
			//const uint32_t funct7 = instr_funct7(id_instr).u;
			const uint8_t rs1 = instr_rs1(instr.instr);
			const uint8_t rs2 = instr_rs2(instr.instr);
			const uint8_t rd = instr_rd(instr.instr);

			const rs_t *rs;
			rs_t *new_rs;
			rs_find_free(curr, next, &rs, &new_rs);

			rob_t *const new_rob = rob_find_free(curr, next);

			bool hold_remaining = false;
			const bool btac_hit = (instr.pc.u == instr.btac.br_pc.u);

			assert(!next->ras_cmd || !opcode);

			next->stats.issued++;
			if (per_pc_stats)
				per_pc_stats[instr.pc.u].issued++;

			switch (opcode) {
			case OPC_LOAD: {
				rs = new_rs = NULL;
				rs_t *const new_ldb = ldb_find_free(curr, next);
				/* Put load in ROB, load buffer. 
				 * Will be executed only when any dependent stores are retired. */
				tracei("(load)\n");

				if (new_ldb && new_rob) {
					ldb_alloc(curr, next, new_ldb);
					rs_rob_alloc(curr, next, new_ldb, new_rob, ROB_INSTR_REGISTER, RS_LOAD,
						instr.pc, instr_lsu_op(opcode, funct3));
					rs_set_rsrc1(new_ldb, rs1, next);

					assert(new_ldb->busy);

					tracei("[id] put in rob %lu", new_ldb->rob_id);

					new_rob->dbg_was_load = 1;
					new_ldb->immediate = instr_imm_itype(instr.instr);
					if (new_ldb->qj == 0) {
						new_ldb->addr.u = new_ldb->vj.u + new_ldb->immediate.u;
						tracei(" with addr %x\n", new_ldb->addr.u);
						if (!new_ldb->addr.u) {
							tracei("[id] Load from null addr\n");
							new_ldb->addr.u = 0xffFFffFF;
						}
					} else {
						tracei(" without addr\n");
						new_ldb->addr.u = 0;
					}

					rob_rd(next, new_rob, rd);
				} else {
					tracei("[id] no free ldb or rob\n");
					hold_remaining = 1;
				}
			}
				break;
			case OPC_STORE: {
				/* Stores retired immediately when ready. */
				tracei("(store)\n");

				/* FIXME: if op1, op2 are already available. */
				if (rs && new_rob) {
  					rs_rob_alloc(curr, next, new_rs, new_rob, ROB_INSTR_STORE, RS_STORE,
							instr.pc, (word_u)1u);
					new_rob->store_op = instr_lsu_op(opcode, funct3).u;
					rs_set_rsrc1(new_rs, rs1, next);
					rs_set_rsrc2(new_rs, rs2, next);

					tracei("[id] put in sb %lu, store from %s\n", new_rs->rob_id, reg_name(rs2));

					new_rs->immediate = instr_imm_stype(instr.instr);
					if (new_rs->qj == 0) {
						new_rs->addr.u = new_rs->vj.u + new_rs->immediate.u;
						if (!new_rs->addr.u) {
							new_rs->addr.u = 0xFFffFFff;	
						}
					} else {
						new_rs->addr.u = 0;
					}
				} else {
					tracei("[id] no free rs\n");
					hold_remaining = 1;
				}
			}
				break;
			case OPC_REG_REG: {
				if (rd == 0) {
					tracei("(reg-reg) nop\n");
					break;
				}
				tracei("(reg-reg)\n");

				if (rs && new_rob) {
					rs_rob_alloc(curr, next, new_rs, new_rob, ROB_INSTR_REGISTER, RS_ALU,
							instr.pc, (word_u){ .u = instr_alu_op(instr.instr) });
					rs_set_rsrc1(new_rs, rs1, next);
					rs_set_rsrc2(new_rs, rs2, next);

					tracei("[id] put in rob id %lu for reg %s\n", new_rob->id, reg_name(rd));

					new_rs->addr.u = new_rs->immediate.u = 0;

					rob_rd(next, new_rob, rd);
				} else {
					tracei("[id] no free rs\n");
					hold_remaining = 1;
				}
			}
				break;
			case OPC_REG_IMM: {
				if (rd == 0) {
					tracei("(reg-imm) nop\n");
					break;
				}
				tracei("(reg-imm)\n");

				if (rs && new_rob) {
					rs_rob_alloc
					(
					 	curr, next, new_rs, new_rob,
						ROB_INSTR_REGISTER, RS_ALU,
						instr.pc,
						(word_u){ .u = instr_alu_op(instr.instr) }
					);
					rs_set_rsrc1(new_rs, rs1, next);

					tracei("[id] put in rob id %lu for reg %s\n", new_rob->id, reg_name(rd));

					new_rs->qk = 0;
				        new_rs->vk = instr_imm_itype(instr.instr);
					new_rs->addr.u = new_rs->immediate.u = 0;

					rob_rd(next, new_rob, rd);
				} else {
					tracei("[id] no free rs\n");
					hold_remaining = 1;
				}
				break;
			}
			case OPC_AUIPC: {
				if (rd == 0) {
					tracei("(auipc) nop\n");
					break;
				}
				tracei("(auipc)\n");

				if (new_rob) {
					/* Assume we can do pc + imm in decode (this is needed
					 * for auipc, branch, jal, so not too far fetched) */
					rob_alloc_only(curr, next, new_rob, ROB_INSTR_REGISTER, instr.pc);
					rob_rd(next, new_rob, rd);
					rob_ready(new_rob, (word_u){ 
						.u = instr.pc.u + instr_imm_utype(instr.instr).u
					});
				} else {
					tracei("[id] no free rob\n");
					hold_remaining = 1;
				}
				break;
			}
			case OPC_LUI:
				/* Just put imm in ROB. */
				tracei("(lui) -- doing wb.\n");
				if (rd == 0) {
					// nop
				} else if (new_rob) {
					rob_alloc_only(curr, next, new_rob, ROB_INSTR_REGISTER, instr.pc);
					rob_rd(next, new_rob, rd);
					rob_ready(new_rob, instr_imm_utype(instr.instr));
				} else {
					tracei("[id] no free ROB\n");
					hold_remaining = 1;
				}
				break;
			case OPC_BRANCH: {
				/* Give BRU op1, op2, target. */
				tracei("(branch) ");
				if (rs && new_rob) {
					const word_u taddr = (word_u) { 
						.u = instr_imm_btype(instr.instr).u + instr.pc.u
					};
					new_rob->dbg_branch_info.type = ROB_BRANCH_CMP;
					/* Branch prediction:
					 * - Do BHT, else static prediction.
					 * - If BTAC missed, but we predict a branch, drop next decode
					 *   and send proper prediction back to fetch. */
					bool p;
					if (btac_hit) {
						if (instr.bht.valid && instr.bht.ctr < 2) {
							tracei("BTAC hit but bht predicts not taken\n");
							p = 0;
							new_rob->dbg_branch_info.pred = ROB_PRED_BHT;
							next->pc_decode_predict.u = instr.pc.u + 4;
						} else {
							tracei("btac hit %x\n", taddr.u);
							new_rob->dbg_branch_info.pred = ROB_PRED_BTAC;
							p = 1;
						}
					} else {
					       	if (instr.bht.valid) {
							p = instr.bht.ctr > 1;
							new_rob->dbg_branch_info.pred = ROB_PRED_BHT;
						} else {
							p = instr_imm_btype(instr.instr).s < 0;
							new_rob->dbg_branch_info.pred = ROB_PRED_STATIC;
						}
						if (p) {
							tracei("pred taken\n");
							next->pc_decode_predict = taddr;
						} else {
							tracei("pred not taken\n");
						}
					}

					rs_rob_alloc(curr, next, new_rs, new_rob, ROB_INSTR_BRANCH, RS_BR,
							instr.pc, (word_u) { .u = BRU_OP_SET | funct3 });
					rs_set_rsrc1(new_rs, rs1, next);
					rs_set_rsrc2(new_rs, rs2, next);

					new_rs->immediate = taddr;
					next->global_branch_history = (curr->global_branch_history << 1) | p;
					if (p) {
						new_rs->predicted_taddr = new_rob->data.brt.pred = taddr;
					} else {
						new_rs->predicted_taddr = new_rob->data.brt.pred =
							(word_u){ .u = instr.pc.u + 4 };
					}
					new_rob->branch_ctrl.change_bht = b_set(1);
					new_rob->branch_ctrl.consider_prediction = b_set(1);
					new_rob->branch_ctrl.pred_taken = b_set(p);

					new_rob->dbg_branch_info.type = ROB_BRANCH_CMP;

					if (opt_nospec) {
						new_rs->predicted_taddr.u = new_rob->data.brt.pred.u = 0;
						next->decode_drop_next = 1;
						next->fetch_wait_jalr_bru = 1;
						next->pc_decode_predict.u = 0;
						new_rob->branch_ctrl.change_bht = b_set(0);
						new_rob->branch_ctrl.consider_prediction = b_set(0);
						new_rob->dbg_branch_info.pred = ROB_PRED_NONE;
					}
				} else {
					tracei("no free rs\n");
					hold_remaining = 1;
				}
				break;
			}
			case OPC_JAL: {
				/* Put branch, then PC to reg on ROB, and change next PC. */
				tracei("(jal)");
				if (rs && new_rob) {
					const bool two = ((next->rob_head + 2) & ROB_INDEX_MASK) != curr->rob_tail;
					if (rd != 0 && !two) {
						goto jal_alloc_fail;
					}

					rob_alloc_only(curr, next, new_rob, ROB_INSTR_BRANCH, instr.pc);

					tracei(" %lu", new_rob->id);

					if (rd != 0) {
						rob_t *rob_two = rob_find_free(curr, next);
						tracei(" %lu", rob_two->id);
						assert(rob_two != new_rob);
						rob_alloc_only(curr, next, rob_two, ROB_INSTR_REGISTER, instr.pc);
						rob_rd(next, rob_two, rd);
						rob_ready(rob_two, (word_u) { .u = instr.pc.u + 4 });
					}
					tracei("\n");
					/* If BTAC hit, then fetch is already in right place.
					 * Otherwise, pass back correct target. */
					const word_u target = (word_u) {
						.u = instr_imm_jtype(instr.instr).u + instr.pc.u
					};
					if (btac_hit) {
						assert(instr.btac.taddr.u == target.u);
						new_rob->dbg_branch_info.pred = ROB_PRED_BTAC;
					} else {
						next->pc_decode_predict = target;
						new_rob->dbg_branch_info.pred = ROB_PRED_NONE;
					}

					new_rob->branch_ctrl.consider_prediction = b_set(0);
					new_rob->branch_ctrl.change_bht = b_set(0);
					new_rob->dbg_branch_info.type = ROB_BRANCH_JAL;
					rob_ready(new_rob, target);

					if (is_link_reg(rd) && !opt_nospec) {
						next->ras_cmd = RAS_PUSH;
						next->ras_arg.u = instr.pc.u + 4;
						if (opt_clearhistoncall)
							next->global_branch_history = 0;
					}
				} else {
				jal_alloc_fail:
					tracei("\n[id] no free rob\n");
					hold_remaining = 1;
				}
				break;
			}
			case OPC_JALR: {
				tracei("(jalr) ");
				if (rs && new_rob) {
					const bool two = ((next->rob_head + 2) & ROB_INDEX_MASK) != curr->rob_tail;
					if (rd != 0 && !two) {
						goto jalr_alloc_fail;
					}
					/* BRU on JALR will take the target address as 
					 * op1 + imm */
					/* In case of BTAC hit, go along with that and rob will deal with mispredict. */
					rs_rob_alloc(curr, next, new_rs, new_rob, ROB_INSTR_BRANCH, RS_BR,
						instr.pc, (word_u){ .u = BRU_OP_JALR_TO_FETCH });
					new_rob->dbg_branch_info.type = ROB_BRANCH_JALR;
					if (!is_link_reg(rd) && is_link_reg(rs1) && curr->ras_head.u) {
						/* Always pop off stack,
						 * if BTAC missed/mispredicted, pass back correct PC. */
						next->ras_cmd = RAS_POP;
						new_rs->predicted_taddr = new_rob->data.brt.pred =
							curr->ras_head;
						new_rs->op.u = BRU_OP_JALR_TO_ROB;
						if (curr->ras_head.u != instr.btac.taddr.u)
							next->pc_decode_predict = curr->ras_head;

						new_rob->branch_ctrl.change_bht = b_set(0);
						new_rob->branch_ctrl.consider_prediction = b_set(1);
						new_rob->dbg_branch_info.pred = ROB_PRED_RAS;
						tracei("ras hit.\n");
					} else if (btac_hit) {
						tracei("btac hit.\n");
						new_rs->predicted_taddr = new_rob->data.brt.pred =
							instr.btac.taddr;
						new_rs->op.u = BRU_OP_JALR_TO_ROB;
						new_rob->branch_ctrl.change_bht = b_set(0);
						new_rob->branch_ctrl.consider_prediction = b_set(1);
						new_rob->dbg_branch_info.pred = ROB_PRED_BTAC;
					} else {
						tracei("btac miss.\n");
						/* Otherwise, we have no idea where to go next,
						 * stall fetch and decode until the branch unit lets fetch know */
						next->decode_drop_next = 1;
						next->fetch_wait_jalr_bru = 1;
						new_rob->branch_ctrl.change_bht = b_set(0);
						new_rob->branch_ctrl.consider_prediction = b_set(0);
						new_rob->dbg_branch_info.pred = ROB_PRED_NONE;
					}
					rs_set_rsrc1(new_rs, rs1, next);
					new_rs->immediate = instr_imm_itype(instr.instr);

					/* 2nd ROB for link reg wb.
					 * Assume we can just tell it out pc+4 right off the bat. */
					if (rd != 0) {
						rob_t *rob_two = rob_find_free(curr, next);
						assert(rob_two != new_rob);
						rob_alloc_only(curr, next, rob_two, ROB_INSTR_REGISTER, instr.pc);
						rob_rd(next, rob_two, rd);
						rob_ready(rob_two, (word_u) { .u = instr.pc.u + 4 });
					}
				} else {
				jalr_alloc_fail:
					tracei("[id] no free rs/rob/etc\n");
					hold_remaining = 1;
				}
				break;
			}
			case OPC_FENCE: {
				tracei("fence (nop)\n");
				break;
			}
			case OPC_ENV:
				tracei("(env)\n");
				switch (instr_imm_utype(instr.instr).u) {
				case 0x0:
					assert(0 && "Ecall not implemented"); 
				case 0x100000:
					if (rs && new_rob) {
						rs_rob_alloc(curr, next, new_rs, new_rob, ROB_INSTR_DEBUG, RS_DBG,
							instr.pc, (word_u)1u);
						/* Reg according to our peverse calling convention. */
						rs_set_rsrc1(new_rs, REG_T3, next);
						rs_set_rsrc2(new_rs, REG_T4, next);
						rob_rd(next, new_rob, REG_T3);
					} else {
						tracei("[id] no free rs\n");
						hold_remaining = 1;
					}
					break;
				default:
					printf("%x\n", instr_imm_utype(instr.instr).u);
					assert(0);
				}
				break;
			case 0x0:
				tracei("(none)\n");
				break;
			default:
				if (new_rob) {
					tracei("(invalid)\n");
					rob_alloc_only(curr, next, new_rob, ROB_INSTR_DEBUG, instr.pc);
					new_rob->ready = 1;
					new_rob->exception = 1;
					fprintf(stderr, "[decode] Warn unknown instr 0x%x at PC %x\n", instr.instr.u, instr.pc.u);
				} else {
					tracei("[id] no free rob\n");
					hold_remaining = 1;
				}
			}

			if (hold_remaining) {
				assert(!next->pc_decode_predict.u);
				next->decode_is_clear = 0;
				tracei("[id] holding from %d\n", i);
				for (size_t j = i; j < ISSUE_WIDTH; j++) {
					next->held_window[j - i] = decode_window[j];
				}
				break;
			} else if (next->pc_decode_predict.u || next->fetch_wait_jalr_bru) {
				break;
			}
		}
		for (size_t i = 0; i < CDB_WIDTH; i++)
			next->cdb[i] = (cdb_t){ 0 };

		/* RAS */
		ras_do(curr, next);

/* Exec. */
		/* One loop per unit - correspoding to an RS_ type. */
		for (size_t i = 0; i < ALU_COUNT; i++) {
			const alu_t *alu = &curr->alus[i];
			alu_t *new = &next->alus[i];
			cdb_t *cdb = cdb_find_free(next);
			if (!alu->rob_id || cdb) {
				const rs_t *rs = rs_waiting_and_free(curr, next, RS_ALU);
				if (rs) {
					tracei("[alu] have instr from %lu\n", rs->rob_id);
					*new = (alu_t) {
						.op = rs->op.u,
						.op1 = rs->vj,
						.op2 = rs->vk,
						.rob_id = rs->rob_id,
						.clk_start = curr->clk,
					};
				} else {
					tracei("[alu] nothing to fetch.\n");
				}
			}
		       	if (alu->rob_id) {
				if (cdb) {
					cdb->rob_id = alu->rob_id;
					cdb->data = alu_do_op(alu->op, alu->op1, alu->op2);
					tracei("[alu] put result (%u %x) into cdb with tag %lu\n",
							cdb->data.u, cdb->data.u, alu->rob_id);
				} else {
					tracei("[alu] stall waiting for cdb with tag %lu\n", alu->rob_id);
					next->stats.wait_cdb++;
					*new = *alu;
				}
			}
		}
		next->ldb_tail = curr->ldb_tail;
		for (size_t i = 0; i < LSU_COUNT; i++) {
			const lsu_t *lsu = &curr->lsus[i];
			lsu_t *new = &next->lsus[i];
			if (!lsu->rob_id) {
				const rs_t *ldb = ldb_next_and_free(curr, next);
				if (ldb) {
					tracei("[ldb] has op from ldb (%lu)\n", ldb->rob_id);
					*new = (lsu_t) {
						.op = ldb->op.u,
						.addr = ldb->addr,
						.rob_id = ldb->rob_id,
						.clk_start = (curr->clk + rand()) & 3,
						.data_in = ldb->vk,
					};
				} else {
					tracei("[ldb] no instr available\n");
				}
			} else if (lsu->data_out_set) {
				cdb_t *cdb = cdb_find_free(next);
				if (cdb) {
					cdb->rob_id = lsu->rob_id;
					cdb->exception = lsu->exception;
					cdb->data = lsu->data_out;
					tracei("[ldb] addr %x put result (%u %x) on cdb with tag %lu\n",
						lsu->addr.u, cdb->data.u, cdb->data.u, cdb->rob_id);
				} else {
					tracei("[ldb] stall waiting for cdb with tag %lu\n", lsu->rob_id);
					next->stats.wait_cdb++;
					*new = *lsu;
				}
			} else {
				*new = *lsu;
				bool have_val = false, wait_val = false;
				word_u val;
				bool overlap = rob_earlier_store_overlaps(curr, lsu, &val, &have_val, &wait_val);
				if (have_val && feature_store_forward) {
					tracei("[ldb] result forwarded from store.\n");
					new->data_out_set = 1;
					new->data_out = val;
				} else if (overlap) {
					if (wait_val)
						next->stats.wait_store_data++;
					else
						next->stats.wait_store_addr++;
					tracei("[ldb] stall waiting for earlier store\n");
				} else if (lsu->clk_start + 2 < curr->clk) {
					tracei("[ldb] Fetch result from mem.\n");
					new->data_out_set = true;
					if (lsu->addr.u == 0xFFffFFff) {
						new->exception = 1;
					} else {
						new->data_out = lsu_do_op(mem, lsu->op, lsu->addr, lsu->data_in, &new->exception);
					}
				}
			}
		}
		for (size_t i = 0; i < BRU_COUNT; i++) {
			const bru_t *bru = &curr->brus[i];
			bru_t *new = &next->brus[i];
			if (!bru->op) {
				tracei("[bru] wait\n");
				const rs_t *rs = rs_waiting_and_free(curr, next, RS_BR);
				if (rs) {
					tracei("[bru] have instr from rs %lu\n", rs->rob_id);
					*new = (bru_t) {
						.op = rs->op.u,
						.op1 = rs->vj,
						.op2 = rs->vk,

						.rob_id = rs->rob_id,

						.pc = rs->pc,
						.imm = rs->immediate,

						.predicted_taddr = rs->predicted_taddr,
					};
				}
			} else {
				cdb_t *cdb = cdb_find_free(next);
				if (cdb) {
					word_u act = bru_act_target(bru);
					cdb->rob_id = bru->rob_id;
					cdb->data = act;
					if (bru->op == BRU_OP_JALR_TO_FETCH || opt_nospec) {
						assert(curr->fetch_wait_jalr_bru || curr->fetch_wait_rob_mispredict);
						assert(bru->rob_id);
						next->pc_exec_bru = act;
						tracei("[bru] set pc_exec_bru for JALR\n");
					}
					/* Can't do anything about issued instrs yet, but
					 * might as well stop fetching new ones. */
					if (act.u != bru->predicted_taddr.u && bru->op != BRU_OP_JALR_TO_FETCH && !opt_nospec) {
						tracei("[bru] stall fetch and decode.\n");
						next->fetch_wait_rob_mispredict = 1;
						next->decode_drop_next = 1;
					}
				} else {
					tracei("[bru] stall for cdb\n");
					next->stats.wait_cdb++;
					*new = *bru;
				}
			}
		}
		/* Store. When addr set, send to ROB and set ready bit.
		 * Assume dedicated bus for this? */
		{
			const rs_t *rs = rs_waiting_and_free(curr, next, RS_STORE);
			if (rs) {
				tracei("[rs] move complete store to ROB\n");
				rob_t *new_rob = &next->rob[rs->rob_id - 1];
				assert(new_rob->id == rs->rob_id);
				assert(rs->addr.u);
				assert(!new_rob->data.reg.dest.u && !new_rob->ready);
				new_rob->data.reg.dest = rs->addr;
				assert(!rs->qk);
				new_rob->data.reg.val = rs->vk;
				new_rob->ready = 1;
			}

		}
		/* Debug / IO unit. All the actual work is done in retire
		 * (speculatively asking for input would be bad), 
		 * so just forward to ROB. */
		{
			const rs_t *rs = rs_waiting_and_free(curr, next, RS_DBG);
			if (rs) {
				tracei("[rs] move complete debug op to ROB\n");
				rob_t *new_rob = &next->rob[rs->rob_id - 1];
				assert(new_rob->id == rs->rob_id);
				new_rob->data.debug.opcode = rs->vj;
				new_rob->data.debug.operand = rs->vk;
				new_rob->ready = 1;
			}
		}
/* Retire */
		memcpy(next->btac, curr->btac, sizeof(curr->btac));
		memcpy(next->bht, curr->bht, sizeof(curr->bht));

		size_t x = 0;
		bool flushed = false;
		bool retired = true;
		for (size_t tail = curr->rob_tail; !flushed && retired; tail = (tail + 1) & ROB_INDEX_MASK) {
			if (tail == curr->rob_head) {
				tracei("[commit] ROB empty\n");
				next->rob_tail = tail;
				break;
			} else if (x++ == RETIRE_WIDTH) {
				next->rob_tail = tail;
				break;
			}

			const rob_t *const entry = &curr->rob[tail];
			rob_t *const new_entry = &next->rob[tail];
			assert(entry->id);

			if (per_pc_stats)
				per_pc_stats[entry->pc.u].rob_type = entry->type;
			if (!entry->ready) {
				tracei("[commit] ROB tail not ready\n");
				next->rob_tail = tail;

				next->stats.stalled++;
				if (per_pc_stats)
					per_pc_stats[entry->pc.u].retire_stall++;
				break;
			}
			if (per_pc_stats)
				per_pc_stats[entry->pc.u].retired++;

			if (entry->exception) {
				printf("[commit] Error: Exception on retire. Either null deref or invalid instr.\n");
				debugger_pause = 1;
				next->rob_tail = tail;
				break;
			}

			if (trace_pc) {
				fprintf(trace_pc, "%x\n", entry->pc.u);
			}

			switch (entry->type) {
			case ROB_INSTR_BRANCH: {
				next->stats.branches++;
				word_u pred = entry->data.brt.pred;
				word_u act = entry->data.brt.act;
				upd_branch_stats(entry, next, per_pc_stats);
				/* If mispredict, flush and change pc. Otherwise, do nothing. */
				bool_t taken = (bool_t) { 0 };
				if (b_test(entry->branch_ctrl.consider_prediction)) {
					if (pred.u != act.u) {
						tracei("[commit] Branch mispredict -- flush pipeline and jmp %x\n",
							act.u);
						assert(pred.u != 0xFFffFFff);
						assert(curr->fetch_wait_rob_mispredict);
						assert(act.u);
						pipeline_flush(next);
						flushed = 1;
						size_t num = 0;
						FOR_INDEX_ROB(curr, i) {
							num++;
						}
						next->stats.flushed += num;
						next->pc_rob_mispredict = act;
						next->global_branch_history = entry->branch_ctrl.global_history;
						taken = b_not(entry->branch_ctrl.pred_taken);
					} else {
						taken = entry->branch_ctrl.pred_taken;
						tracei("[commit] Correct branch prediction\n");
					}
				}
				if (b_test(entry->branch_ctrl.change_bht)) {
					bht_btac_update(curr, next, entry->pc, entry->branch_ctrl.global_history, b_test(taken), act);
					
				} else {
					btac_update(next, entry->pc, act);
				}
				break;
			} case ROB_INSTR_REGISTER: {
				if (entry->dbg_was_load)
					next->stats.loads++;
				else
					next->stats.arithmetic++;
				word_u dest = entry->data.reg.dest;
				word_u val = entry->data.reg.val;
				/* Just wb to reg. */
				tracei("[commit] %lu wb %.2X to reg %s",
					entry->id, val.u, reg_name(dest.u));
				assert(dest.u && dest.u < REG_COUNT);
				assert(curr->arf[dest.u].rob_id && next->arf[dest.u].rob_id);

				next->arf[dest.u].dat = val;
				if (next->arf[dest.u].rob_id == entry->id) {
					tracei("(+ reset)\n");
					next->arf[dest.u].rob_id = 0;
				} else {
					tracei("\n");
				}
				break;
			} case ROB_INSTR_STORE: {
				next->stats.stores++;
				word_u dest = entry->data.reg.dest;
				word_u val = entry->data.reg.val;
				tracei("[commit] Store val %x to addr %x\n", val.u, dest.u);
				if (dest.u == 0xFFffFFff) {
					fprintf(stderr, "[commit] pc %x store to null ptr.\n", entry->pc.u);
					debugger_pause = 1;
				} else if (dest.u < bin_region) {
					tracei("[commit] note: pc %x store to code region (%x).\n", entry->pc.u, dest.u);
				}
				bool exception = false;
				lsu_do_op(mem, entry->store_op, dest, val, &exception);
				if (exception) {
					fprintf(stderr, "[commit] exception attempting write to %x\n", dest.u);
					debugger_pause = 1 && (!permissive);
				}
				break;
			} case ROB_INSTR_DEBUG: {
				next->stats.env++;
				cdb_t *cdb = cdb_find_free(next);
				if (!cdb) {
					tracei("[dbgu] Wait on CDB for possible wb.\n");
					retired = false;
					break;
				}

				cdb->rob_id = entry->id;

				word_u operand = entry->data.debug.operand;
				/* All cases except input are relatively straightforward. */
				switch (entry->data.debug.opcode.u) {
				case DBG_OP_BREAK:
					printf("[dbgu] have break instr\n");
					debugger_pause = 1;
					break;
				case DBG_OP_QUIT:
					printf("[dbgu] quit\n");
					run = 0;
					break;
				case DBG_OP_ABORT:
					printf("[dbgu] assertion failed\n");
					debugger_pause = 1;
					break;
				case DBG_OP_PRINT:
					printf("[dbgu] msg: '%s'\n", (char*)&mem[operand.u]);
					break;
				case DBG_OP_BENCH_BEGIN:
					assert(!next->stats.start_clk);
					next->stats = (struct stats){ 0 };
					next->stats.start_clk = curr->clk;
					printf("[dbgu] bench start at clk %lu\n", next->stats.start_clk);
					if (trace_pc)
						trace_pc = freopen(NULL, "wb", trace_pc);
					memset(next->bht, 0, sizeof(next->bht));
					memset(next->btac, 0, sizeof(next->btac));
					break;
				case DBG_OP_BENCH_END:
					assert(next->stats.start_clk);
					printf("[dbgu] Bench end\n");
					if (bench_only)
						run = 0;
					else
						debugger_pause = 1;
					next->stats.start_clk = 0;
					break;
				case DBG_OP_INPUT:
					printf("[dbgu] Programme requesting char input: ");
					fflush(stdout);
					cdb->data.u = getchar();
					getchar(); // Clear newline.
					break;
				default:
					assert(0 && "Programme broke debug calling convention.");
				}
				break;
			} default:
				assert(0);
			}

			if (retired) {
				*new_entry = (rob_t) { 0 };
				next->stats.retired++;
			} else {
				*new_entry = *entry;
				next->rob_tail = tail;
			}
		}
	}

	free(mem);
	if (curr) {
		{
			size_t r = curr->stats.retired,
			       i = curr->stats.issued,
			       f = curr->stats.flushed,
			       c = curr->clk - curr->stats.start_clk,
			       ib = curr->stats.branches,
			       il = curr->stats.loads,
			       is = curr->stats.stores,
			       ie = curr->stats.env,
			       ia = curr->stats.arithmetic,
			       st = curr->stats.stall_mispredict,
			       
			       wa = curr->stats.wait_args,
			       wx = curr->stats.wait_ex,
			       wc = curr->stats.wait_cdb,
			       wla = curr->stats.wait_store_addr,
			       wld = curr->stats.wait_store_data;

			double ipc = (double)r / (double)c,
				ipc_b = (double)r / (double)(c - st),
				fw_sz = (double)curr->stats.fetch_window_sum / (double)curr->stats.fetch_window_cnt,
				waf = (double)wa / (double)i,
				wxf = (double)wx / (double)i,
				wcf = (double)wc / (double)i,
				wlaf = (double)wla / (double)i,
				wldf = (double)wld / (double)i;

			printf("Benchmark retired %lu and flushed %lu in %lu clocks\n", r, f, c);
			printf("Issued %lu\n", i);
			printf("Waited: %lu (%f) for args, %lu (%f) for exec unit, %lu (%f) for cdb, %lu (%f) on store addr, %lu (%f) on store data.\n",
					wa, waf,
					wx, wxf,
					wc, wcf,
					wla, wlaf,
					wld, wldf);
			printf("Spent %lu (%f) cycles stalled from mispredict.\n", st, (double)st / (double)c);
			printf("IPC: %f (%f excluding mispredict penalty)\n", ipc, ipc_b);
			printf("Avg decode window: %f\n", fw_sz);
			printf("Max recursion: %lu\n", curr->stats.recursion_depth_max);
			const char *fmt = "%s:\t%lu\t(%f%%)\n";
			printf(fmt, "Loads", il, (double)il*100. / (double)r);
			printf(fmt, "Stores", is, (double)is*100. / (double)r);
			printf(fmt, "Branches", ib, (double)ib*100. / (double)r);
			printf(fmt, "Arithmetic", ia, (double)ia*100. / (double)r);
			printf(fmt, "Env", ie, (double)ie*100. / (double)r);
		}


		const char *stat_fmt = "\t%s:\t\t%lu (%.1f%%)\t%lu\t%lu\t\t%.2f%%\n";

		printf("\nBranch prediction (possibly limited to benchmark segment):\n");
		printf("\t\t\tNum\t\tCorrect\tMisspredict\tCorrect predict rate\n");
		{
			size_t  h = curr->stats.jal_btac_hits,
				m = curr->stats.jal_btac_miss,
				t = h + m;
			double hr = 100. * (double)h / (double)t,
			       mr = 100. * (double)m / (double)t;
			printf("JAL:\n");
			printf(stat_fmt, "BTAC", h, hr, 0, 0, 0.);
			printf(stat_fmt, "None", m, mr, 0, 0, 0.);
		}
		printf("JALR:\n");
		{
			size_t  ac = curr->stats.jalr_btac_correct,
				rc = curr->stats.jalr_ras_correct,
				ai = curr->stats.jalr_btac_incorrect,
				ri = curr->stats.jalr_ras_incorrect,
				a = ac + ai,
				r = rc + ri,
				m = curr->stats.jalr_btac_miss,
				t = a + m + r;
			double	ar = 100. * (double)a / (double)t,
				rr = 100. * (double)r / (double)t,
				mr = 100. * (double)m / (double)t,
				acr = 100. * (double)ac / (double)a,
				rcr = 100. * (double)rc / (double)r;
			printf(stat_fmt, "BTAC", a, ar, ac, ai, acr);
			printf(stat_fmt, "RAS ", r, rr, rc, ri, rcr);
			printf(stat_fmt, "None", m, mr, 0, 0, 0.);
		}
		printf("Conditional:\n");
		{
			size_t 	c = curr->stats.bht_conflicts,

				hc = curr->stats.cmp_bht_correct,
				hi = curr->stats.cmp_bht_incorrect,
				ac = curr->stats.cmp_btac_correct,
				ai = curr->stats.cmp_btac_incorrect,
				sc = curr->stats.cmp_static_correct,
				si = curr->stats.cmp_static_incorrect,
				tc = hc + ac + sc,
				ti = hi + ai + si,
				h = hc + hi,
				a = ac + ai,
				s = sc + si,
				t = h + a + s;
			double cr = 100. * (double)c / (double)t,

				hr = 100. * (double)h / (double)t,
				ar = 100. * (double)a / (double)t,
				sr = 100. * (double)s / (double)t,

				hcr = 100. * (double)hc / (double)h,
				acr = 100. * (double)ac / (double)a,
				scr = 100. * (double)sc / (double)s,
				tcr = 100. * (double)tc / (double)t;

			printf(stat_fmt, "All", t, 100.0, tc, ti, tcr);
			printf(stat_fmt, opt_nospec ? "None" : "Static", s, sr, sc, si, scr);
			printf(stat_fmt, "BHT", h, hr, hc, hi, hcr);
			printf(stat_fmt, "BTAC", a, ar, ac, ai, acr);

			printf("BHT conflicts: %lu / %lu (%f%%)\n", c, t, cr);
		}

		if (per_pc_stats) {
			for (size_t pc = 0; pc < bin_size; pc++) {
				const struct per_pc_stats s = per_pc_stats[pc];
				if (!s.rob_type)
					continue;

				printf("%f - RETIRE: %lx %s : retired %lu stalled %lu (%f)\n",
					(double)s.retire_stall / (double)curr->stats.stalled, 
					pc, rob_type_str(s.rob_type), s.retired, s.retire_stall,
					(double)s.retire_stall / (double)s.retired
				);

				printf("%f - ISSUE: %lx %s : issued %lu stalled for args %lu exec %lu (%f)\n",
					(double)(s.ex_stall + s.arg_stall) / (double)curr->clk,
					pc, rob_type_str(s.rob_type),
					s.issued,
					s.arg_stall,
					s.ex_stall,
					(double)(s.ex_stall + s.arg_stall) / (double)s.issued
				);

				double r;
				switch (s.type) {
				case ROB_BRANCH_INVALID:
					break;
				case ROB_BRANCH_CMP:
					r = (double)s.bht_correct / (double)(s.bht_correct + s.bht_incorrect);
					printf("%lu - CMP BHT: %lx : %lu\t%lu (%f)\n", s.bht_correct + s.bht_incorrect, pc, s.bht_correct, s.bht_incorrect, r);
					break;
				default:
					break;
				}
			}
		} else {
			printf("No granular stats (add granular to args)\n");
		}

		free((void*)curr);
	}

	if (next) {
		free(next);
	}

	if (trace_pc) {
		fclose(trace_pc);
	}

	return 0;
}

