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
#include <sbi/sbi_console.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hsm.h>
//#include <atcsmu.h>
#include <andes_ae350.h>

/*
 * PLIC
 */

#define PLIC_SOURCES			71
#define PLIC_IE_WORDS			((PLIC_SOURCES + 31) / 32)

static u8 plic_priority[PLIC_SOURCES];
static u32 plic_sie[PLIC_IE_WORDS];
static u32 plic_threshold;

// static void ae350_plic_save(void)
void ae350_plic_save(void)
{
	fdt_plic_context_save(true, plic_sie, &plic_threshold);
	fdt_plic_priority_save(plic_priority);
}

// static void ae350_plic_restore(void)
void ae350_plic_restore(void)
{
	fdt_plic_priority_restore(plic_priority);
	fdt_plic_context_restore(true, plic_sie, plic_threshold);
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
#define PCS0_WE_OFF     0x90
#define PCS0_CTL_OFF    0x94
#define PCSm_WE_OFF(i)  ((i + 3) * 0x20 + PCS0_WE_OFF)
#define PCSm_CTL_OFF(i) ((i + 3) * 0x20 + PCS0_CTL_OFF)
#define SLEEP_CMD       0x3
#define LIGHTSLEEP_MODE  0
#define DEEPSLEEP_MODE   1
#define PCS_CTL_PARAM_OFF 0x3
	unsigned long smu_base = 0xf0100000;
	u32 hartid = current_hartid();
	unsigned int events = 0;
	switch (suspend_type) {
		case SBI_HSM_SUSPEND_RET_PLATFORM:
			// 1. Set proper interrupts in PLIC and wakeup events in PCSm_WE
			events = (1 << 9) | (1 << 2);
			events = (hartid == 0) ? (events | (1 << 28)) : events;
			events = 0xffffffff;
			volatile char *smu_we_base = (void *)(smu_base + PCSm_WE_OFF(hartid));
			writel(events, smu_we_base);
			// 2. Write the light sleep command to PCSm_CTL
			unsigned long smu_val = 0;
			smu_val = SLEEP_CMD | (LIGHTSLEEP_MODE << PCS_CTL_PARAM_OFF); 

			volatile char *smu_pcs_ctl_base = (void *)(smu_base + PCSm_CTL_OFF(hartid));
			writel(smu_val, smu_pcs_ctl_base);
			// 3. Disable all clocks of a core
			// 3.1 Flush D-cache
			asm volatile ("csrw 0x7cc, 0x6\n"); // 0x7cc: mcctlcommand, 0x6: L1D_WBINVAL_ALL
			// 3.2 Disable D-cache
			asm volatile ("csrc 0x7ca, 0x2\n"); // mcache_ctl.DC_EN = 0 [FIXME]: disable I-cache here?
			// 3.3 Disable D-cache coherency
			asm volatile ("li t1, 0x80000\n"  // mcache_ctl.DC_COHEN = 0
						  "csrc 0x7ca, t1\n");
			// 3.4 Wait for mcache_ctl.DC_COHSTA to be cleared
			asm volatile (
					"1:	csrr t1, 0x7ca\n"
					"	srli t1, t1, 12\n"
					"	li t5, 0x100\n"
					"	and t1, t1, t5\n"
					"	bnez t1, 1b\n");
			wfi();
			// 1. Resume: Eable all clocks of a core
			// 1.1 Enable D-cache coherency
			asm volatile ("li t1, 0x80000\n" // mcache_ctl.DC_COHEN = 1
						  "csrs 0x7ca, t1\n");
			// 1.2 Wait for mcache_ctl.DC_COHSTA to be set
			asm volatile (
					"1:	csrr t1, 0x7ca\n"
					"	srli t1, t1, 12\n"
					"	li t5, 0x100\n"
					"	and t1, t1, t5\n"
					"	beqz t1, 1b\n");
			// 1.3 Enable D-cache
			asm volatile("csrs 0x7ca, 0x2");
			break;
		case SBI_HSM_SUSPEND_NON_RET_PLATFORM:
			// sbi_printf("[%s] hart%d: SBI_HSM_SUSPEND_NON_RET_PLATFORM\n", __func__, hartid);
			// 456
			// ae350_plic_save();
			wfi();
			// ae350_plic_restore();
			// sbi_hart_hang();
			break;
		default:
			/*
			 * Unsupported suspend type, fall through to default
			 * retentive suspend
			 */
			return SBI_ENOTSUPP;
	}

	return 0;
}

/**
 * Perform platform-specific actions to resume from a suspended state.
 *
 * This includes restoring any platform state that was lost during
 * non-retentive suspend.
 */
void ae350_hart_resume(void)
{
	// sbi_printf("[hart%d/%s] Andes AE350 Non-retentive mode (DeepSleep)\n",
	// 		current_hartid(), __FUNCTION__);
}

static const struct sbi_hsm_device andes_smu = {
	.name = "andes_smu test",
	.hart_start = NULL,
	.hart_stop = NULL,
	.hart_suspend = ae350_hart_suspend,
	.hart_resume = ae350_hart_resume,
};

struct smu_data {
	unsigned long addr;
} smu;
static void ae350_hsm_device_init(void) {
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
	{ },
};

const struct platform_override andes_ae350 = {
	.match_table = andes_ae350_match,
	.final_init = ae350_final_init,
};
