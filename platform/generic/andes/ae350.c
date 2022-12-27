/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Andes Technology Corporation
 *
 * Authors:
 *   Yu Chien Peter Lin <peterlin@andestech.com>
 */

#include <platform_override.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi_utils/irqchip/fdt_irqchip_plic.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_ipi.h>

#include <andes/atcsmu.h>
#include <andes/andes45.h>

struct smu_data smu;
extern void __ae350_enable_coherency(void);
extern void __ae350_enable_coherency_warmboot(void);
extern void __ae350_disable_coherency(void);

static __always_inline bool is_andes25(void)
{
	uintptr_t marchid = csr_read(CSR_MARCHID);
	return EXTRACT_FIELD(marchid, CSR_MARCHID_MICROID) == 0xa25;
}

static void smu_set_wakeup_events(u32 events, u32 hartid)
{
	writel(events, (void *)(smu.addr + PCSm_WE_OFFSET(hartid)));
}

static bool smu_support_sleep_mode(u32 sleep_mode, u32 hartid)
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
	case DEEPSLEEP_MODE:
		if (EXTRACT_FIELD(pcs_cfg, PCS_CFG_DEEP_SLEEP) == 0) {
			sbi_printf(
				"SMU: hart%d (PCS%d) does not support deep sleep mode\n",
				hartid, hartid + 3);
			return false;
		}
	};

	return true;
}

static void smu_set_command(u32 pcs_ctl, u32 hartid)
{
	writel(pcs_ctl, (void *)(smu.addr + PCSm_CTL_OFFSET(hartid)));
}

static void smu_wait_clear_command(u32 hartid)
{
	ulong pcs_ctl_addr = smu.addr + PCSm_CTL_OFFSET(hartid);

	do {
		writel(0x0, (void *)pcs_ctl_addr);
	} while (readl((void *)pcs_ctl_addr));
}

static void smu_set_reset_vector(ulong wakeup_addr, u32 hartid)
{
	writel(wakeup_addr,
	       (void *)(smu.addr + SMU_HARTn_RESET_VEC_LO(hartid)));
	writel((u64)wakeup_addr >> 32,
	       (void *)(smu.addr + SMU_HARTn_RESET_VEC_HI(hartid)));
}

int ae350_hart_suspend(u32 suspend_type)
{
	u32 hartid;

	hartid = current_hartid();

	switch (suspend_type) {
	case SBI_HSM_SUSPEND_RET_PLATFORM:
		if (!smu_support_sleep_mode(LIGHTSLEEP_MODE, hartid))
			return SBI_ENOTSUPP;

		smu_set_wakeup_events(0xffffffff, hartid);
		smu_set_command(LIGHT_SLEEP_CMD, hartid);
		__ae350_disable_coherency();

		wfi();

		__ae350_enable_coherency();

		if (is_andes25())
			smu_wait_clear_command(hartid);
		break;
	default:
		/**
		 * Unsupported suspend type, fall through to default
		 * retentive suspend
		 */
		return SBI_ENOTSUPP;
	}

	return 0;
}

int ae350_hart_start(u32 hartid, ulong saddr)
{
	/**
	 * Hotplugging hart 0 of 25-series falls through warmboot
	 * flow as it can not sleep, other hart sends IPI to bring
	 * hart 0 online.
	 */
	if (is_andes25() && hartid == 0)
		return sbi_ipi_raw_send(hartid);

	/* Send wakeup command to the sleep hart */
	smu_set_command(WAKEUP_CMD, hartid);

	return 0;
}

int ae350_hart_stop(void)
{
	u32 hartid = current_hartid();

	/**
	 * The hart0 shares power domain with L2-cache,
	 * instead of turning it off, it fall through and
	 * jump to warmboot_addr.
	 */
	if (is_andes25() && hartid == 0)
		return SBI_ENOTSUPP;

	if (!smu_support_sleep_mode(DEEPSLEEP_MODE, hartid))
		return SBI_ENOTSUPP;

	/**
	 * disable all events, the current hart will be
	 * woken up from reset vector by other hart via
	 * writing its PCS (power control slot) control
	 * register
	 */
	smu_set_wakeup_events(0x0, hartid);
	smu_set_command(DEEP_SLEEP_CMD, hartid);
	smu_set_reset_vector((ulong)__ae350_enable_coherency_warmboot,
			       hartid);
	__ae350_disable_coherency();

	wfi();

	sbi_hart_hang();
	return 0;
}

static const struct sbi_hsm_device andes_smu = {
	.name	      = "andes_smu",
	.hart_start   = ae350_hart_start,
	.hart_stop    = ae350_hart_stop,
	.hart_suspend = ae350_hart_suspend,
	.hart_resume  = NULL,
};

static void ae350_hsm_device_init(void)
{
	int rc;
	void *fdt;

	fdt = fdt_get_address();

	rc = fdt_parse_compat_addr(fdt, (uint64_t *)&smu.addr,
				   "andestech,atcsmu");

	if (!rc) {
		sbi_hsm_set_device(&andes_smu);
	}
}

static int ae350_final_init(bool cold_boot, const struct fdt_match *match)
{
	if (cold_boot)
		ae350_hsm_device_init();

	return 0;
}

static const struct fdt_match andes_ae350_match[] = {
	{ .compatible = "andestech,ae350" },
	{},
};

const struct platform_override andes_ae350 = {
	.match_table = andes_ae350_match,
	.final_init  = ae350_final_init,
};
