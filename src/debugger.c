#include "debugger.h"

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
			if (next->bht.buffer[i].valid) {
				printf("%x: %d\n", next->bht.buffer[i].debug_last_pc.u, next->bht.buffer[i].ctr);
			}
		}
	} else if (strcmp(arg, "ras") == 0) {
		printf("Head: %lu, %x\n", next->ras.head_ptr, next->ras.head.u);
		for (size_t i = next->ras.head_ptr; ((i - 1) & RAS_INDEX_MASK) != next->ras.head_ptr; i = (i - 1) & RAS_INDEX_MASK) {
			printf("%lu - %x\n", i, next->ras.buffer[i].u);
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


