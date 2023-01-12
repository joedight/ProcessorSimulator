#include "stats.h"

#include "config.h"

#include <stdio.h>

void stats_print(const struct stats *stats, size_t clk)
{
	{
		size_t r = stats->retired,
		       i = stats->issued,
		       f = stats->flushed,
		       c = clk - stats->start_clk,
		       ib = stats->branches,
		       il = stats->loads,
		       is = stats->stores,
		       ie = stats->env,
		       ia = stats->arithmetic,
		       st = stats->stall_mispredict,
		       
		       wa = stats->wait_args,
		       wx = stats->wait_ex,
		       wc = stats->wait_cdb,
		       wla = stats->wait_store_addr,
		       wld = stats->wait_store_data;

		double ipc = (double)r / (double)c,
			ipc_b = (double)r / (double)(c - st),
			fw_sz = (double)stats->fetch_window_sum / (double)stats->fetch_window_cnt,
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
		printf("Max recursion: %lu\n", stats->recursion_depth_max);
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
		size_t  h = stats->jal_btac_hits,
			m = stats->jal_btac_miss,
			t = h + m;
		double hr = 100. * (double)h / (double)t,
		       mr = 100. * (double)m / (double)t;
		printf("JAL:\n");
		printf(stat_fmt, "BTAC", h, hr, 0, 0, 0.);
		printf(stat_fmt, "None", m, mr, 0, 0, 0.);
	}
	printf("JALR:\n");
	{
		size_t  ac = stats->jalr_btac_correct,
			rc = stats->jalr_ras_correct,
			ai = stats->jalr_btac_incorrect,
			ri = stats->jalr_ras_incorrect,
			a = ac + ai,
			r = rc + ri,
			m = stats->jalr_btac_miss,
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
		size_t 	c = stats->bht_conflicts,

			hc = stats->cmp_bht_correct,
			hi = stats->cmp_bht_incorrect,
			ac = stats->cmp_btac_correct,
			ai = stats->cmp_btac_incorrect,
			sc = stats->cmp_static_correct,
			si = stats->cmp_static_incorrect,
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
}


