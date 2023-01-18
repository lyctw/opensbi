/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Andes Technology Corporation
 */

#include <sbi_utils/sys/atcsmu.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_error.h>
#include <andes/andes45.h>

extern struct smu_data smu;

void smu_set_wakeup_events(u32 events, u32 hartid)
{
	writel(events, (void *)(smu.addr + PCSm_WE_OFFSET(hartid)));
}

bool smu_support_sleep_mode(u32 sleep_mode, u32 hartid)
{
	u32 pcs_cfg;

	pcs_cfg = readl((void *)(smu.addr + PCSm_CFG_OFFSET(hartid)));

	switch (sleep_mode) {
	case LIGHTSLEEP_MODE:
		if (EXTRACT_FIELD(pcs_cfg, PCS_CFG_LIGHT_SLEEP) == 0) {
			sbi_printf(
				"SMU: hart%d (PCS%d) does not support light sleep mode\n",
				hartid, hartid + 3);
			return false;
		}
		break;
	case DEEPSLEEP_MODE:
		if (EXTRACT_FIELD(pcs_cfg, PCS_CFG_DEEP_SLEEP) == 0) {
			sbi_printf(
				"SMU: hart%d (PCS%d) does not support deep sleep mode\n",
				hartid, hartid + 3);
			return false;
		}
		break;
	}

	return true;
}

void smu_set_command(u32 pcs_ctl, u32 hartid)
{
	writel(pcs_ctl, (void *)(smu.addr + PCSm_CTL_OFFSET(hartid)));
}

void smu_set_reset_vector(ulong wakeup_addr, u32 hartid)
{
	writel(wakeup_addr,
	       (void *)(smu.addr + HARTn_RESET_VEC_LO(hartid)));
	writel((u64)wakeup_addr >> 32,
	       (void *)(smu.addr + HARTn_RESET_VEC_HI(hartid)));
}
