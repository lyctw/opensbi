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
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_ipi.h>
#include <sbi/sbi_scratch.h>
#include <andes/atcsmu.h>
#include <andes/andes45.h>

struct smu_data smu;
extern void __ae350_enable_clk(void);
extern void __ae350_enable_clk_warmboot(void);
extern void __ae350_disable_clk(void);

static __always_inline bool is_andes25(void)
{
	uintptr_t marchid = csr_read(CSR_MARCHID);
	return EXTRACT_FIELD(marchid, CSR_MARCHID_MICROID) == 0xa25;
}
/*
 * Section 20.5: Disable all clocks of a core
 * - Disable D-cache coherency by applying the sequence in Section 20.4.1
 *   - Disable D-cache by clearing mcache_ctl.DC_EN. This step changes accesses to
 *     non-cacheable regions for preventing D-cache from allocating more lines.
 *   - Write back and invalidate D-cache by executing the CCTL command L1D_WBINVAL_ALL.
 *     All modified lines are flushed and D-cache will not cache stale lines after power-up
 *   - Disable D-cache coherency by clearing mcache_ctl_.DC_COHEN. CM will not send probe
 *     requests while D-cache is powered down
 *   - Wait for mcache_ctl.DC_COHSTA to be cleared to ensure the previous step is completed
 * - Execute the WFI instruction and wait for coreN_wfi_mode to be asserted
 * - Clocks should not be disabled until the processor enters WFI mode
 */

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
			return 0;
		}
	case DEEPSLEEP_MODE:
		if (EXTRACT_FIELD(pcs_cfg, PCS_CFG_DEEP_SLEEP) == 0) {
			sbi_printf(
				"SMU: hart%d (PCS%d) does not support deep sleep mode\n",
				hartid, hartid + 3);
			return 0;
		}
	};

	return 1;
}

static void __naked smu_set_command(u32 pcs_ctl, u32 hartid)
{
	/* Program the sleep command to PCS_CTL register */
	//writel(pcs_ctl, (void *)(smu.addr + PCSm_CTL_OFFSET(hartid)));
	asm volatile (
			/* PCSm_CTL_OFFSET is 0x94 + 0x20 * (hartid + 3) */
			"	addi t1, a1, 3\n"
			"	slli t1, t1, 5\n"
			"	addi t1, t1, 0x94\n"
			/* read base address stored in smu.addr */
			"	la t2, smu\n"
			"	"REG_L" t2, 0(t2)\n"
			/* smu.addr + PCSm_CTL_OFFSET(hartif) */
			"	add t1, t1, t2\n"
			/* write command to PCSm_CTL register */
			"	sw a0, 0(t1)\n"
			"	ret\n");
}

static void smu_set_wakeup_addr(ulong wakeup_addr, u32 hartid)
{
	writel(wakeup_addr,
	       (void *)(smu.addr + SMU_HARTn_RESET_VEC_LO(hartid)));
	writel(wakeup_addr >> 32,
	       (void *)(smu.addr + SMU_HARTn_RESET_VEC_HI(hartid)));
}

/**
 * Put the current hart in platform specific suspend (or low-power)
 * state.
 *
 * For successful retentive suspend, the call will return 0 when
 * the hart resumes normal execution.
 *
 * For successful non-retentive suspend, the hart will resume from
 * the warm boot entry point.
 */
int ae350_hart_suspend(u32 suspend_type)
{
	u32 hartid;

	hartid = current_hartid();

	switch (suspend_type) {
	case SBI_HSM_SUSPEND_RET_PLATFORM:
		if(!smu_support_sleep_mode(LIGHTSLEEP_MODE, hartid))
			return SBI_ENOTSUPP;
		// 1. Set proper interrupts in PLIC and wakeup events in PCSm_WE
		smu_set_wakeup_events(0xffffffff, hartid);
		// 2. Write the light sleep command to PCSm_CTL
		smu_set_command(LIGHT_SLEEP_CMD, hartid);
		// 3. Disable all clocks of a core
		__ae350_disable_clk();

		wfi();
		// 1. Resume: Eable all clocks of a core
		__ae350_enable_clk();
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

/** Start (or power-up) the given hart */
int ae350_hart_start(u32 hartid, ulong saddr)
{
	sbi_printf(
		"%s(hartid=%d, saddr=0x%lx): hart%d is sending wakeup command to hard%d\n",
		__func__, hartid, saddr, current_hartid(), hartid);

	/* Send wakeup command to the sleep hart */
	writel(WAKEUP_CMD, (void *)(smu.addr + PCSm_CTL_OFFSET(hartid)));

	return 0;
}

/**
 * Stop (or power-down) the current hart from running. This call
 * doesn't expect to return if success.
 */
int ae350_hart_stop(void)
{
	u32 hartid = current_hartid();

	if(!smu_support_sleep_mode(DEEPSLEEP_MODE, hartid))
		return SBI_ENOTSUPP;

	/**
	 * The hart0 shares power domain with L2-cache,
	 * so we can not turn it off. Fall through and
	 * jump to warmboot_addr in this case.
	 */
	if(is_andes25() && hartid == 0) {
		sbi_printf("[%s] I'm 25 hart 0, fall through to generic flow\n",
				__func__);
		return SBI_ENOTSUPP;
	}

	// 1. Set M-mode software interrupt wakeup events in PCSm_WE
	//    disable any event, the only way to bring it up is sending
	//    wakeup command through PCSm_CTL of the sleep hart
	smu_set_wakeup_events(0x0, hartid);
	// 2. Write the deep sleep command to PCSm_CTL
//	rc = smu_set_command(DEEP_SLEEP_CMD, hartid);
//	if (rc)
//		return SBI_ENOTSUPP;
	/* Set wakeup address for sleep hart */
	smu_set_wakeup_addr((ulong)__ae350_enable_clk_warmboot, hartid);
	// 3. Disable all clocks of a core
	__ae350_disable_clk();


	smu_set_command(DEEP_SLEEP_CMD, hartid);

	wfi();

	/* 
	 * Should wakeup from warmboot, the deep sleep
	 * hart's reset vector is set to __ae350_enable_clk_warmboot
	 * when ae350_hart_start is called by other hart
	 */
	sbi_hart_hang();
	return 0;
}

static const struct sbi_hsm_device andes_smu = {
	.name	      = "andes_smu XDD3",
	.hart_start   = ae350_hart_start,
	.hart_stop    = ae350_hart_stop,
	.hart_suspend = NULL,
	.hart_resume  = NULL,
};

static void ae350_hsm_device_init(void)
{
	int rc;
	void *fdt;

	fdt = fdt_get_address();

	rc = fdt_parse_compat_addr(fdt, (uint64_t *)&smu.addr,
				   "andestech,atcsmu");
	if (!rc)
		sbi_hsm_set_device(&andes_smu);
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
