#include "pipeline.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "debugger.h"

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
				const cdb_entry *cdb = NULL;
				if (old->qj && (cdb = cdb_with_rob(&curr->cdb, old->qj))) {
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
				if (old->qk && (cdb = cdb_with_rob(&curr->cdb, old->qk))) {
					tracei("[rs] writeback op2 to %lu from %lu\n", old->rob_id, old->qk);
					assert(old->busy);
					new->qk = 0;
					new->vk = cdb->data;
				}
				assert(!cdb_with_rob(&curr->cdb, old->rob_id));
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

			const cdb_entry *cdb = cdb_with_rob(&curr->cdb, old->id);
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
					.instr = memory_op(mem, LSU_OP_LW, pc, (word_u){ .u = 0 }, &exception),
					.btac = curr->btac.buffer[(pc.u / 4) & BTAC_INDEX_MASK],
					.bht = curr->bht.buffer[bht_index(pc, curr->global_branch_history)],
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

			assert(!next->ras.cmd || !opcode);

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
						next->ras.cmd = RAS_PUSH;
						next->ras.arg.u = instr.pc.u + 4;
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
					if (!is_link_reg(rd) && is_link_reg(rs1) && curr->ras.head.u) {
						/* Always pop off stack,
						 * if BTAC missed/mispredicted, pass back correct PC. */
						next->ras.cmd = RAS_POP;
						new_rs->predicted_taddr = new_rob->data.brt.pred =
							curr->ras.head;
						new_rs->op.u = BRU_OP_JALR_TO_ROB;
						if (curr->ras.head.u != instr.btac.taddr.u)
							next->pc_decode_predict = curr->ras.head;

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

		cdb_clear(&next->cdb);

		/* RAS */
		ras_do(&curr->ras, &next->ras);

/* Exec. */
		/* One loop per unit - correspoding to an RS_ type. */
		for (size_t i = 0; i < ALU_COUNT; i++) {
			const alu_t *alu = &curr->alus[i];
			alu_t *new = &next->alus[i];
			cdb_entry *cdb = cdb_find_free(&next->cdb);
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
					cdb->data = alu_result(alu);
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
				cdb_entry *cdb = cdb_find_free(&next->cdb);
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
						new->data_out = memory_op(mem, lsu->op, lsu->addr, lsu->data_in, &new->exception);
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
				cdb_entry *cdb = cdb_find_free(&next->cdb);
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
		next->btac = curr->btac;
		next->bht = curr->bht;
//		memcpy(next->btac, curr->btac, sizeof(curr->btac));
//		memcpy(next->bht, curr->bht, sizeof(curr->bht));

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
					btac_update(&next->btac, entry->pc, act);
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
				memory_op(mem, entry->store_op, dest, val, &exception);
				if (exception) {
					fprintf(stderr, "[commit] exception attempting write to %x\n", dest.u);
					debugger_pause = 1 && (!permissive);
				}
				break;
			} case ROB_INSTR_DEBUG: {
				next->stats.env++;
				cdb_entry *cdb = cdb_find_free(&next->cdb);
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
					next->bht = (struct bht){ 0 };
					next->btac = (struct btac){ 0 };
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
		stats_print(&curr->stats, curr->clk);
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

