/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Andes Technology Corporation
 *
 * Authors:
 *   Zong Li <zong@andestech.com>
 *   Nylon Chen <nylon7@andestech.com>
 *   Leo Yu-Chi Liang <ycliang@andestech.com>
 *   Yu Chien Peter Lin <peterlin@andestech.com>
 */

#include <sbi/sbi_error.h>
#include <sbi/sbi_ipi.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/ipi/fdt_ipi.h>
#include <sbi_utils/ipi/andes_plicsw.h>

struct plicsw_data plicsw;

static struct sbi_ipi_device plicsw_ipi = {
	.name = "andes_plicsw",
	.ipi_send = plicsw_ipi_send,
	.ipi_clear = plicsw_ipi_clear
};

static int plicsw_warm_ipi_init(void)
{
	u32 hartid = current_hartid();

	/* Clear PLICSW IPI */
	plicsw_ipi_clear(hartid);

	return 0;
}

static int plicsw_cold_ipi_init(void *fdt, int nodeoff,
			      const struct fdt_match *match)
{
	int rc;

	rc = fdt_parse_plicsw_node(fdt, nodeoff, &plicsw.addr,
			&plicsw.size, &plicsw.num_src, &plicsw.hart_count);
	if (rc)
		return rc;

	/* Setup source priority */
	uint32_t *priority = (void *)plicsw.addr + PLICSW_PRIORITY_BASE;

	for (int i = 0; i < plicsw.hart_count; i++)
		writel(1, &priority[i]);

	/* Setup target enable */
	uint32_t enable_mask = PLICSW_HART_MASK;

	for (int i = 0; i < plicsw.hart_count; i++) {
		uint32_t *enable = (void *)plicsw.addr + PLICSW_ENABLE_BASE
			+ PLICSW_ENABLE_STRIDE * i;
		writel(enable_mask, enable);
		writel(enable_mask, enable + 1);
		enable_mask <<= 1;
	}

	sbi_ipi_set_device(&plicsw_ipi);

	return 0;
}

static const struct fdt_match ipi_plicsw_match[] = {
	{ .compatible = "riscv,plic1" },
	{ },
};

struct fdt_ipi fdt_ipi_plicsw = {
	.match_table = ipi_plicsw_match,
	.cold_init = plicsw_cold_ipi_init,
	.warm_init = plicsw_warm_ipi_init,
	.exit = NULL,
};
