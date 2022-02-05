#include <stdbool.h>

enum {
	PIPELINE_WIDTH = 4,

	REG_COUNT = 32,

	ALU_COUNT = PIPELINE_WIDTH,
	LSU_COUNT = 2,
	BRU_COUNT = 1,

	ISSUE_WIDTH = PIPELINE_WIDTH,
	RETIRE_WIDTH = PIPELINE_WIDTH,

	RS_COUNT = 24,

	LDB_SIZE = 8,
	LDB_INDEX_MASK = LDB_SIZE - 1,

	ROB_SIZE = 32,
	ROB_INDEX_MASK = ROB_SIZE - 1,

	BHT_SIZE = 128,
	BHT_INDEX_MASK = BHT_SIZE - 1,

	GLOBAL_HISTORY_BITS = 3,
	GLOBAL_HISTORY_MASK = (1 << GLOBAL_HISTORY_BITS) - 1,

	BTAC_SIZE = 32,
	BTAC_INDEX_MASK = BTAC_SIZE - 1,

	RAS_SIZE = 4,
	RAS_INDEX_MASK = RAS_SIZE - 1,

	CDB_WIDTH = PIPELINE_WIDTH,
};

static bool feature_2level = true;
static bool feature_store_forward = true;
static bool feature_branch_bht_btac = true;

static bool opt_clearhistoncall = false;
static bool opt_1bitbht = false;
static bool opt_nospec = false;
static bool opt_gshare = false;
static bool opt_nostorechk = false;
