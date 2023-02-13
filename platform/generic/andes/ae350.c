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
#include <sbi_utils/sys/atcsmu.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_ipi.h>
#include <sbi/sbi_init.h>
#include <sbi/sbi_console.h> // !!! DROP!!!
#include <sbi/sbi_system.h>
#include <andes/andes45.h>

static struct smu_data smu = { 0 };
extern void __ae350_enable_coherency_warmboot(void);
extern void __ae350_enable_coherency(void);
extern void __ae350_disable_coherency(void);

static __always_inline bool is_andes25(void)
{
	ulong marchid = csr_read(CSR_MARCHID);
	return !!(EXTRACT_FIELD(marchid, CSR_MARCHID_MICROID) == 0xa25);
}

static int ae350_hart_start(u32 hartid, ulong saddr)
{
	sbi_printf("%s(): hart%d is reuqesting start to hart%d\n", __func__, current_hartid(), hartid);

	/* Don't send wakeup command at boot-time */
	if (!sbi_init_count(hartid) || (is_andes25() && hartid == 0))
		return sbi_ipi_raw_send(hartid);

	/* Write wakeup command to the sleep hart */
	smu_set_command(&smu, WAKEUP_CMD, hartid);

	return 0;
}

static int ae350_hart_stop(void)
{
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();
	void (*jump_warmboot)(void) = (void (*)(void))scratch->warmboot_addr;

	u32 hartid = current_hartid();
	sbi_printf("%s(): hart%d\n", __func__, current_hartid());
	smu_set_wakeup_events(&smu, 0x0, hartid);
	smu_set_command(&smu, LIGHT_SLEEP_CMD, hartid);
//	smu_set_reset_vector(&smu, (ulong)__ae350_enable_coherency_warmboot,
//			       hartid);

	__ae350_disable_coherency();
	
	wfi();

	__ae350_enable_coherency();

	jump_warmboot();

	sbi_panic("Should not panic here\n");
	return SBI_ENOTSUPP;

#if 0
	int rc;
	u32 hartid = current_hartid();

	/**
	 * For Andes AX25MP, the hart0 shares power domain with
	 * L2-cache, instead of turning it off, it should fall
	 * through and jump to warmboot_addr.
	 */
	if (is_andes25() && hartid == 0)
		return SBI_ENOTSUPP;

	if (!smu_support_sleep_mode(&smu, DEEPSLEEP_MODE, hartid))
		return SBI_ENOTSUPP;

	/**
	 * disable all events, the current hart will be
	 * woken up from reset vector when other hart
	 * writes its PCS (power control slot) control
	 * register
	 */
	smu_set_wakeup_events(&smu, 0x0, hartid);
	smu_set_command(&smu, DEEP_SLEEP_CMD, hartid);

	rc = smu_set_reset_vector(&smu, (ulong)__ae350_enable_coherency_warmboot,
			       hartid);
	if (rc)
		goto fail;

	__ae350_disable_coherency();

	wfi();

fail:
	/* It should never reach here */
	sbi_hart_hang();
	return 0;
#endif
}

static const struct sbi_hsm_device andes_smu = {
	.name	      = "andes_smu",
	.hart_start   = ae350_hart_start,
	.hart_stop    = ae350_hart_stop,
};

//static void ae350_hsm_device_init(void)
//{
//	int rc;
//	void *fdt;
//
//	fdt = fdt_get_address();
//
//	rc = fdt_parse_compat_addr(fdt, (uint64_t *)&smu.addr,
//				   "andestech,atcsmu");
//}

static int ae350_system_suspend_test_check(u32 sleep_type)
{
	return sleep_type == SBI_SUSP_SLEEP_TYPE_SUSPEND;
}

static int ae350_system_suspend_test_suspend(u32 sleep_type)
{
//#define WAKEUP_EVENTS (1 << 9) // UART2
#define WAKEUP_EVENTS 0xffffffff
	u32 hartid;
//	int rc;

	if (sleep_type != SBI_SUSP_SLEEP_TYPE_SUSPEND)
		return SBI_EINVAL;

	hartid = current_hartid();
	smu_check_pcs_status(&smu, hartid, LIGHTSLEEP_MODE); // block if other hart still active
	smu_set_wakeup_events(&smu, WAKEUP_EVENTS, hartid);
	smu_set_command(&smu, LIGHT_SLEEP_CMD, hartid);
//	rc = smu_set_reset_vector(&smu, (ulong)__ae350_enable_coherency_warmboot,
//			       hartid);
//	if (rc)
//		goto fail;

	__ae350_disable_coherency();

  /* Wait for interrupt */
	wfi();

	__ae350_enable_coherency();

	return SBI_OK;

//fail:
//	sbi_panic("ERROR %s(): failed to set reset vector\n", __func__);
}

static struct sbi_system_suspend_device ae350_system_suspend_test = {
	.name = "ae350_suspend_test",
	.system_suspend_check = ae350_system_suspend_test_check,
	.system_suspend = ae350_system_suspend_test_suspend,
};

static int ae350_final_init(bool cold_boot, const struct fdt_match *match)
{
	int rc;
	void *fdt;

	if (cold_boot) {
		fdt = fdt_get_address();

		rc = fdt_parse_compat_addr(fdt, (uint64_t *)&smu.addr,
					   "andestech,atcsmu");
		if(rc)
			sbi_panic("Fail to parse atcsmu\n");
		sbi_hsm_set_device(&andes_smu);
		//ae350_hsm_device_init();
		sbi_system_suspend_set_device(&ae350_system_suspend_test);
	}

	return 0;
}

static const struct fdt_match andes_ae350_match[] = {
	{ .compatible = "andestech,ae350" },
	{ },
};

const struct platform_override andes_ae350 = {
	.match_table = andes_ae350_match,
	.final_init  = ae350_final_init,
};
