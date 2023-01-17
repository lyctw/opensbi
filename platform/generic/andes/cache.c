/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Andes Technology Corporation
 *
 * Authors:
 *   Nylon Chen <nylon7@andestech.com>
 */

int mcall_icache_op(unsigned int enable)
{
	if (enable)
		csr_set(CSR_MCACHE_CTL, V5_MCACHE_CTL_IC_EN);
	else {
		csr_clear(CSR_MCACHE_CTL, V5_MCACHE_CTL_IC_EN);
		RISCV_FENCE_I;
	}
	return 0;
}

int mcall_dcache_wbinval_all(void)
{
	csr_write(CSR_MCCTLCOMMAND, V5_UCCTL_L1D_WBINVAL_ALL);

	return 0;
}


