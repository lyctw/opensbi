/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sbi/riscv_encoding.h>
#include <sbi/riscv_asm.h>
#include <andes/andes45.h>

	.section .text, "ax", %progbits
	.align 3
	.global __ae350_disable_coherency
__ae350_disable_coherency:
	/* flush D-cache */
	csrw	CSR_MCCTLCOMMAND, 0x6
	/* disable I/D-cache */
	csrc	CSR_MCACHECTL, 0x3
	/* disable D-cache coherency */
	lui	t1, 0x80
	csrc	CSR_MCACHECTL, t1
	/* 45-series: wait for mcache_ctl.DC_COHSTA to be cleared */
check_cm_disabled:
	csrr	t1, CSR_MCACHECTL
	srli	t1, t1, 20
	andi	t1, t1, 0x1
	bnez	t1, check_cm_disabled

	ret

	.section .text, "ax", %progbits
	.align 3
	.global __ae350_enable_coherency
__ae350_enable_coherency:
	/* Enable D-cache coherency */
	lui		t1, 0x80
	csrs	CSR_MCACHECTL, t1
	/*
	* check CM support
	* 25-series: mcache_ctl.DC_COHEN is hard-wired to 0
	*/
	csrr	t1, CSR_MCACHECTL
	srli	t1, t1, 20
	andi	t1, t1, 0x1
	beqz	t1, enable_L1_cache
	/* 45-series: wait for mcache_ctl.DC_COHSTA to be set */
check_cm_enabled:
	csrr	t1, CSR_MCACHECTL
	srli	t1, t1, 20
	andi	t1, t1, 0x1
	beqz	t1, check_cm_enabled
enable_L1_cache:
	/* enable I/D-cache */
	csrs	CSR_MCACHECTL, 0x3

	ret

	.section .text, "ax", %progbits
	.align 3
	.global __ae350_enable_coherency_warmboot
__ae350_enable_coherency_warmboot:
	call ra, __ae350_enable_coherency
	j _start_warm
