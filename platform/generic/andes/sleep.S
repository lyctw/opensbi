/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Andes Technology Corporation
 *
 * Authors:
 *   Yu Chien Peter Lin <peterlin@andestech.com>
 */

#include <sbi/riscv_encoding.h>
#include <sbi/riscv_asm.h>
#include <andes/andes45.h>

	.section .text, "ax", %progbits
	.align 3
	.global __ae350_disable_coherency
__ae350_disable_coherency:
	/* flush d-cache */
	csrw	CSR_MCCTLCOMMAND, 0x6
	/* disable i/d-cache */
	csrc	CSR_MCACHE_CTL, 0x3
	/* disable d-cache coherency */
	lui	t1, 0x80
	csrc	CSR_MCACHE_CTL, t1
	/* 45-series: wait for mcache_ctl.DC_COHSTA to be cleared */
check_cm_disabled:
	csrr	t1, CSR_MCACHE_CTL
	srli	t1, t1, 20
	andi	t1, t1, 0x1
	bnez	t1, check_cm_disabled

	ret

	.section .text, "ax", %progbits
	.align 3
	.global __ae350_enable_coherency
__ae350_enable_coherency:
	/* enable d-cache coherency */
	lui		t1, 0x80
	csrs	CSR_MCACHE_CTL, t1
	/*
	 * check CM support
	 * 25-series: mcache_ctl.DC_COHEN is hard-wired to 0
	 */
	csrr	t1, CSR_MCACHE_CTL
	srli	t1, t1, 20
	andi	t1, t1, 0x1
	beqz	t1, enable_L1_cache
	/* 45-series: wait for mcache_ctl.DC_COHSTA to be set */
check_cm_enabled:
	csrr	t1, CSR_MCACHE_CTL
	srli	t1, t1, 20
	andi	t1, t1, 0x1
	beqz	t1, check_cm_enabled
enable_L1_cache:
	/* enable i/d-cache */
	csrs	CSR_MCACHE_CTL, 0x3

	ret

	.section .text, "ax", %progbits
	.align 3
	.global __ae350_enable_coherency_warmboot
__ae350_enable_coherency_warmboot:
	call ra, __ae350_enable_coherency
	j _start_warm
