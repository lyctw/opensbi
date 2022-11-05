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
#include <sbi/sbi_console.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hsm.h>

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
	/* Use the generic code for retentive suspend. */
	if (!(suspend_type & SBI_HSM_SUSP_NON_RET_BIT))
		return SBI_ENOTSUPP;

	// sbi_printf("[hart%d/%s] Andes AE350 Non-retentive mode (DeepSleep)\n",
	// 		current_hartid(), __FUNCTION__);

	wfi();
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
	.name = "andes_smu",
	.hart_start = NULL,
	.hart_stop = NULL,
	.hart_suspend = ae350_hart_suspend,
	.hart_resume = ae350_hart_resume,
};

static int ae350_final_init(bool cold_boot, const struct fdt_match *match)
{
	if (cold_boot)
		sbi_hsm_set_device(&andes_smu);

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
