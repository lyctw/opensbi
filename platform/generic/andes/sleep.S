#include <sbi/riscv_encoding.h>
#include <andes/andes45.h>

	.section .text, "ax", %progbits
	.align 3
	.global __ae350_disable_clk
__ae350_disable_clk:
	# 3.1 Flush D-cache
	csrw CSR_MCCTLCOMMAND, 0x6  # 0x7cc: mcctlcommand, 0x6: L1D_WBINVAL_ALL
	# 3.2 Disable I/D-cache
	csrc CSR_MCACHECTL, 0x3 # mcache_ctl.DC_EN = 0
	# 3.3 Disable D-cache coherency
	lui t1, 0x80 # mcache_ctl.DC_COHEN = 0
	csrc CSR_MCACHECTL, t1
	# 3.4 Wait for mcache_ctl.DC_COHSTA to be cleared
check_cm_disabled:
	csrr t1, CSR_MCACHECTL
	srli t1, t1, 20
	andi t1, t1, 0x1
	bnez t1, check_cm_disabled

	/*
	* [FIXME]
	* Insert few cycles for hardware handshaking
	*/
	nop
	nop
	nop
	nop
	nop

	ret

	.section .text, "ax", %progbits
	.align 3
	.global __ae350_enable_clk
__ae350_enable_clk:
	/* 1.1 Enable D-cache coherency */
	lui		t1, 0x80
	csrs	CSR_MCACHECTL, t1
	/*
	* Check CM support
	* mcache_ctl.DC_COHEN is hard-wired to 0 on 25-series
	*/
	csrr	t1, CSR_MCACHECTL
	srli	t1, t1, 20
	andi	t1, t1, 0x1
	beqz	t1, enable_L1_cache
	/* 1.2 Wait for mcache_ctl.DC_COHSTA to be set */
check_cm_enabled:
	csrr	t1, CSR_MCACHECTL
	srli	t1, t1, 20
	andi	t1, t1, 0x1
	beqz	t1, check_cm_enabled
enable_L1_cache:
	/* 1.3 enable I/D-cache */
	csrs	CSR_MCACHECTL, 0x3

	ret

	.section .text, "ax", %progbits
	.align 3
	.global __ae350_enable_clk_warmboot
__ae350_enable_clk_warmboot:
	call ra, __ae350_enable_clk
	j _start_warm
