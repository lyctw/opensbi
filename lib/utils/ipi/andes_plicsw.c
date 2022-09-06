/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Andes Technology Corporation
 *
 * Authors:
 *   Zong Li <zong@andestech.com>
 *   Nylon Chen <nylon7@andestech.com>
 *   Yu Chien Peter Lin <peterlin@andestech.com>
 */

#include <sbi/sbi_types.h>
#include <sbi_utils/ipi/andes_plicsw.h>

extern struct plicsw_data plicsw;

static inline void plicsw_claim(void)
{
	u32 hartid = current_hartid();

	if (plicsw.hart_count <= hartid)
		ebreak();

	plicsw.source_id[hartid] =
		readl((void *)plicsw.addr + PLICSW_CONTEXT_BASE +
		      PLICSW_CONTEXT_CLAIM + PLICSW_CONTEXT_STRIDE * hartid);
}

static inline void plicsw_complete(void)
{
	u32 hartid = current_hartid();
	u32 source = plicsw.source_id[hartid];

	writel(source, (void *)plicsw.addr + PLICSW_CONTEXT_BASE +
			       PLICSW_CONTEXT_CLAIM +
			       PLICSW_CONTEXT_STRIDE * hartid);
}

static inline void plic_sw_pending(u32 target_hart)
{
	/*
	 * The pending array registers are w1s type.
	 * IPI pending array mapping as following:
	 *
	 * Pending array start address: base + 0x1000
	 * ---------------------------------
	 * | hart3 | hart2 | hart1 | hart0 |
	 * ---------------------------------
	 * Each hartX can send IPI to another hart by setting the
	 * bitY to its own region (see the below).
	 *
	 * In each hartX region:
	 * <---------- PICSW_PENDING_STRIDE -------->
	 * | bit7 | ... | bit3 | bit2 | bit1 | bit0 |
	 * ------------------------------------------
	 * The bitY of hartX region indicates that hartX sends an
	 * IPI to hartY.
	 */
	u32 hartid	    = current_hartid();
	u32 word_index	    = hartid / 4;
	u32 per_hart_offset = PLICSW_PENDING_STRIDE * hartid;
	u32 val		    = 1 << target_hart << per_hart_offset;

	writel(val, (void *)plicsw.addr + PLICSW_PENDING_BASE + word_index * 4);
}

void plicsw_ipi_send(u32 target_hart)
{
	if (plicsw.hart_count <= target_hart)
		ebreak();

	/* Set PLICSW IPI */
	plic_sw_pending(target_hart);
}

void plicsw_ipi_clear(u32 target_hart)
{
	if (plicsw.hart_count <= target_hart)
		ebreak();

	/* Clear PLICSW IPI */
	plicsw_claim();
	plicsw_complete();
}
