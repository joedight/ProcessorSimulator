#pragma once
#include <stddef.h>

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
};

void stats_print(const struct stats *stats, size_t clk);

