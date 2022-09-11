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

#include <sbi/sbi_domain.h>
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

static int andes_plicsw_add_regions(unsigned long addr, unsigned long size)
{
#define PLICSW_ADD_REGION_ALIGN 0x1000
	int rc;
	unsigned long pos, end, region_size;
	struct sbi_domain_memregion reg;

	pos = addr;
	end = addr + size;
	while (pos < end) {
		if (pos & (PLICSW_ADD_REGION_ALIGN - 1))
			region_size = 1UL << sbi_ffs(pos);
		else
			region_size = ((end - pos) < PLICSW_ADD_REGION_ALIGN)?
				(end - pos) : PLICSW_ADD_REGION_ALIGN;

		sbi_domain_memregion_init(pos, region_size,
			SBI_DOMAIN_MEMREGION_MMIO, &reg);
		rc = sbi_domain_root_add_memregion(&reg);
		if (rc)
			return rc;
		pos += region_size;
	}

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

	/* Add PLICSW region to the root domain */
	rc = andes_plicsw_add_regions(plicsw.addr, plicsw.size);
	if (rc)
		return rc;

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
