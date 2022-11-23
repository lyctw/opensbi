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
#include <sbi/sbi_ipi.h>
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

#define PCS0_WE_OFF     0x90
#define PCS0_CTL_OFF    0x94
#define PCSm_WE_OFF(i)  ((i + 3) * 0x20 + PCS0_WE_OFF)
#define PCSm_CTL_OFF(i) ((i + 3) * 0x20 + PCS0_CTL_OFF)
#define SLEEP_CMD       0x3
#define WAKEUP_CMD      0x8
#define LIGHTSLEEP_MODE  0
#define DEEPSLEEP_MODE   1
#define PCS_CTL_PARAM_OFF 0x3
#define SMU_RESET_VEC_LO_OFF 0x50
#define SMU_RESET_VEC_HI_OFF 0x60
#define SMU_HARTn_RESET_VEC_LO(n) (SMU_RESET_VEC_LO_OFF + (n * 0x4))
#define SMU_HARTn_RESET_VEC_HI(n) (SMU_RESET_VEC_HI_OFF + (n * 0x4))
unsigned long smu_base = 0xf0100000;
volatile char *smu_we_base = NULL;
volatile char *smu_pcs_ctl_base = NULL;
unsigned long smu_val = 0;
unsigned int events = 0;

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
	u32 hartid = current_hartid();
	switch (suspend_type) {
		case SBI_HSM_SUSPEND_RET_PLATFORM:
			// 1. Set proper interrupts in PLIC and wakeup events in PCSm_WE
			events = 0xffffffff;
			smu_we_base = (void *)(smu_base + PCSm_WE_OFF(hartid));
			writel(events, smu_we_base);
			// 2. Write the light sleep command to PCSm_CTL
			smu_val = SLEEP_CMD | (LIGHTSLEEP_MODE << PCS_CTL_PARAM_OFF); 

			smu_pcs_ctl_base = (void *)(smu_base + PCSm_CTL_OFF(hartid));
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
		default:
			/*
			 * Unsupported suspend type, fall through to default
			 * retentive suspend
			 */
			return SBI_ENOTSUPP;
	}

	return 0;
}

/** Start (or power-up) the given hart */
extern void cpu_dcache_enable(void);
int ae350_hart_start(u32 hartid, ulong saddr)
{
	sbi_printf("hart%d should wakeup hart%d from 0x%lx by sending wakeup command\n",
			current_hartid(), hartid, saddr);

	/* Set wakeup address for sleep hart */
	ulong wakeup_addr = (ulong)cpu_dcache_enable;
	sbi_printf("hart%d is writing 0x%lx to 0x%lx\n",
			current_hartid(), wakeup_addr, smu_base + SMU_HARTn_RESET_VEC_LO(hartid));
	writel(wakeup_addr, (void *)(smu_base + SMU_HARTn_RESET_VEC_LO(hartid)));
	writel(wakeup_addr >> 32, (void *)(smu_base + SMU_HARTn_RESET_VEC_HI(hartid)));
	
	/* Send wakeup command to the sleep hart */
	smu_pcs_ctl_base = (void *)(smu_base + PCSm_CTL_OFF(hartid));
	writel(WAKEUP_CMD, smu_pcs_ctl_base);

	return 0;
}

/**
 * Stop (or power-down) the current hart from running. This call
 * doesn't expect to return if success.
 */
int ae350_hart_stop(void)
{
	u32 hartid = current_hartid();
	unsigned long dcache_cm = 0;
	// 1. Set M-mode software interrupt wakeup events in PCSm_WE
	//    disable any event, the only way to bring it up is sending
	//    wakeup command through PCSm_CTL of the sleep hart
	events = 0x0; //0x20000000;
    smu_we_base = (void *)(smu_base + PCSm_WE_OFF(hartid));
	writel(events, smu_we_base);
	// 2. Write the deep sleep command to PCSm_CTL
	smu_val = SLEEP_CMD | (DEEPSLEEP_MODE << PCS_CTL_PARAM_OFF);
	smu_pcs_ctl_base = (void *)(smu_base + PCSm_CTL_OFF(hartid));
	writel(smu_val, smu_pcs_ctl_base);
	// 3. Disable all clocks of a core
	// 3.2 Disable D-cache
	csr_clear(0x7ca, 0x2); // mcache_ctl.DC_EN = 0 [FIXME]: disable I-cache here?
	// 3.1 Flush D-cache
	csr_write(0x7cc, 0x6); // 0x7cc: mcctlcommand, 0x6: L1D_WBINVAL_ALL
	// 3.3 Disable D-cache coherency
	csr_clear(0x7ca, 0x80000); // mcache_ctl.DC_COHEN = 0
	// 3.4 Wait for mcache_ctl.DC_COHSTA to be cleared
	do {
		dcache_cm = csr_read(0x7ca) & 0x100000; 
	} while (dcache_cm);

	wfi();

	/* 
	 * Should wakeup from warmboot, the deep sleep
	 * hart's reset vector is set to saddr given
	 * by ae350_hart_start called by other hart
	 */
	sbi_hart_hang();
	return 0;
}

static const struct sbi_hsm_device andes_smu = {
	.name = "andes_smu XDD",
	.hart_start = ae350_hart_start,
	.hart_stop = ae350_hart_stop,
	.hart_suspend = ae350_hart_suspend,
	.hart_resume = NULL,
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
