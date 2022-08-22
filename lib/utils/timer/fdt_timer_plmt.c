/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Andes Technology Corporation
 *
 * Authors:
 *   Yu Chien Peter Lin <peterlin@andestech.com>
 */

#include <libfdt.h>
#include <sbi/sbi_error.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/timer/fdt_timer.h>
#include <sbi_utils/timer/andes_plmt.h>

extern struct plmt_data plmt;

static struct sbi_timer_device plmt_timer = {
       .name = "andes_plmt",
       .timer_freq = DEFAULT_AE350_PLMT_FREQ,
       .timer_value = plmt_timer_value,
       .timer_event_start = plmt_timer_event_start,
       .timer_event_stop = plmt_timer_event_stop
};

static int plmt_warm_timer_init(void)
{
	if (!plmt.time_val)
		return SBI_ENODEV;

	plmt_timer_event_stop();

	return 0;
}

static int plmt_cold_timer_init(void *fdt, int nodeoff,
						const struct fdt_match *match)
{
	int rc;
	unsigned long freq, plmt_base, plmt_size;

	fdt_parse_plmt_node(fdt, nodeoff, &plmt_base, &plmt_size,
				  &plmt.hart_count);

	plmt.size     = plmt_size;
	plmt.time_val = (u64 *)plmt_base;
	plmt.time_cmp = (u64 *)(plmt_base + 0x8);

	rc = fdt_parse_timebase_frequency(fdt, &freq);
	if (rc)
		return rc;

	plmt_timer.timer_freq = freq;

	sbi_timer_set_device(&plmt_timer);

	return 0;
}

static const struct fdt_match timer_plmt_match[] = {
	{ .compatible = "riscv,plmt0" },
	{ },
};

struct fdt_timer fdt_timer_plmt = {
	.match_table = timer_plmt_match,
	.cold_init = plmt_cold_timer_init,
	.warm_init = plmt_warm_timer_init,
	.exit = NULL,
};
