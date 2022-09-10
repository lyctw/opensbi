/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Andes Technology Corporation
 *
 * Authors:
 *   Zong Li <zong@andestech.com>
 *   Nylon Chen <nylon7@andestech.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_console.h>
#include "plicsw.h"
#include "platform.h"

static u32 plicsw_ipi_hart_count;
static struct plicsw plicsw_dev[AE350_HART_COUNT];

/* #define DEBUG */
static inline void plicsw_claim(void)
{
	u32 source_hart = current_hartid();

	plicsw_dev[source_hart].source_id =
		readl(plicsw_dev[source_hart].plicsw_claim);
#ifdef DEBUG
	sbi_printf("[%s] hart%d is claiming source_id by reading 0x%p and get source_id (pending bit index) = %d\n",
			__FUNCTION__, source_hart, plicsw_dev[source_hart].plicsw_claim, plicsw_dev[source_hart].source_id);
#endif
}

static inline void plicsw_complete(void)
{
	u32 source_hart = current_hartid();
	u32 source = plicsw_dev[source_hart].source_id;

	writel(source, plicsw_dev[source_hart].plicsw_claim);
#ifdef DEBUG
	sbi_printf("[%s] hart%d has completed interrupt handling and writing source_id (pending bit index) = %d to 0x%p\n",
			__FUNCTION__, source_hart, source, plicsw_dev[source_hart].plicsw_claim);
#endif
}

static inline void plic_sw_pending(u32 target_hart)
{
	/*
	 * The pending array registers are w1s type.
	 * IPI pending array mapping as following:
	 *
	 * Pending array start address: base + 0x1000
	 * -------------------------------------
	 * | hart 3 | hart 2 | hart 1 | hart 0 |
	 * -------------------------------------
	 * Each hart X can send IPI to another hart by setting the
	 * corresponding bit in hart X own region(see the below).
	 *
	 * In each hart region:
	 * -----------------------------------------------
	 * | bit 7 | bit 6 | bit 5 | bit 4 | ... | bit 0 |
	 * -----------------------------------------------
	 * The bit 7 is used to send IPI to hart 0
	 * The bit 6 is used to send IPI to hart 1
	 * The bit 5 is used to send IPI to hart 2
	 * The bit 4 is used to send IPI to hart 3
	 */
	u32 source_hart = current_hartid();
	u32 target_offset = (PLICSW_PENDING_PER_HART - 1) - target_hart;
	u32 per_hart_offset = PLICSW_PENDING_PER_HART * source_hart;
	u32 val = 1 << target_offset << per_hart_offset;

	writel(val, plicsw_dev[source_hart].plicsw_pending);
#ifdef DEBUG
	sbi_printf("[%s] hart%d is pending target hart%d by writing 0x%x to hart%d's pending reg @ 0x%p\n",
			__FUNCTION__, source_hart, target_hart,
			val, source_hart, plicsw_dev[source_hart].plicsw_pending);
#endif
}

void plicsw_ipi_send(u32 target_hart)
{
	if (plicsw_ipi_hart_count <= target_hart)
		return;

	/* Set PLICSW IPI */
	plic_sw_pending(target_hart);
}

void plicsw_ipi_clear(u32 target_hart)
{
	if (plicsw_ipi_hart_count <= target_hart)
		return;

	/* Clear PLICSW IPI */
	plicsw_claim();
	plicsw_complete();
}

int plicsw_warm_ipi_init(void)
{
	u32 hartid = current_hartid();

	if (!plicsw_dev[hartid].plicsw_pending
	    && !plicsw_dev[hartid].plicsw_enable
	    && !plicsw_dev[hartid].plicsw_claim)
		return -1;

	/* Clear PLICSW IPI */
	plicsw_ipi_clear(hartid);

	return 0;
}

int plicsw_cold_ipi_init(unsigned long base, u32 hart_count)
{
	/* Setup source priority */
	uint32_t *priority = (void *)base + PLICSW_PRIORITY_BASE;

	for (int i = 0; i < AE350_HART_COUNT; i++)
		writel(1, &priority[i]);

#ifdef DEBUG
	for (int i = 0; i < 8; i++)
		sbi_printf("[%s] 0x%p\tInterrupt source %d priority\t0x%x\n",
				__FUNCTION__, &priority[i], i + 1, readl(&priority[i]));
#endif 
	/* Setup target enable */
	uint32_t enable_mask = PLICSW_HART_MASK;

	for (int i = 0; i < AE350_HART_COUNT; i++) {
		uint32_t *enable = (void *)base + PLICSW_ENABLE_BASE
			+ PLICSW_ENABLE_PER_HART * i;
		writel(enable_mask, &enable[0]);
		enable_mask >>= 1;
	}

#ifdef DEBUG
	enable_mask = PLICSW_HART_MASK;
	for(int cntxid = 0; cntxid < 8; cntxid++) {
		volatile void *plic_ie;
		plic_ie = (char *)base + PLICSW_ENABLE_BASE
			+ PLICSW_ENABLE_PER_HART * cntxid;
		sbi_printf("[%s] 0x%p Interrupt Source #0 to #31 Enables Bits on context %d (hart%d M-mode): \t0x%x\n",
				__FUNCTION__, plic_ie, cntxid, cntxid, readl(plic_ie));
	}
#endif 

	/* Figure-out PLICSW IPI register address */
	plicsw_ipi_hart_count = hart_count;

	for (u32 hartid = 0; hartid < AE350_HART_COUNT; hartid++) {
		plicsw_dev[hartid].source_id = 0;
		plicsw_dev[hartid].plicsw_pending =
			(void *)base
			+ PLICSW_PENDING_BASE
			+ ((hartid / 4) * 4);
		plicsw_dev[hartid].plicsw_enable  =
			(void *)base
			+ PLICSW_ENABLE_BASE
			+ PLICSW_ENABLE_PER_HART * hartid;
		plicsw_dev[hartid].plicsw_claim   =
			(void *)base
			+ PLICSW_CONTEXT_BASE
			+ PLICSW_CONTEXT_CLAIM
			+ PLICSW_CONTEXT_PER_HART * hartid;
	}

#ifdef DEBUG
	for (u32 hartid = 0; hartid < AE350_HART_COUNT; hartid++) {
		sbi_printf("[%s] plicsw_dev[%d].source_id: \t0x%x\n",
				__FUNCTION__, hartid, plicsw_dev[hartid].source_id);
		sbi_printf("[%s] plicsw_dev[%d].plicsw_pending: \t0x%p\n",
				__FUNCTION__, hartid, plicsw_dev[hartid].plicsw_pending);
		sbi_printf("[%s] plicsw_dev[%d].plicsw_enable: \t0x%p\n",
				__FUNCTION__, hartid, plicsw_dev[hartid].plicsw_enable);
		sbi_printf("[%s] plicsw_dev[%d].plicsw_claim: \t0x%p\n",
				__FUNCTION__, hartid, plicsw_dev[hartid].plicsw_claim);
	}
#endif

	return 0;
}
