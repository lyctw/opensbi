#include <sbi/riscv_encoding.h>
#include <sbi/sbi_scratch.h>
#include <atcsmu.h>

###################################
# void __ae350_disable_clk(void) #
###################################
	.section .text, "ax", %progbits
	.align 3
	.global __ae350_disable_clk
__ae350_disable_clk:
	# 3.1 Flush D-cache
	csrw 0x7cc, 0x6  # 0x7cc: mcctlcommand, 0x6: L1D_WBINVAL_ALL
	# 3.2 Disable I/D-cache
	csrc 0x7ca, 0x3 # mcache_ctl.DC_EN = 0
	# 3.3 Disable D-cache coherency
	li t1, 0x80000 # mcache_ctl.DC_COHEN = 0
	csrc 0x7ca, t1
	# 3.4 Wait for mcache_ctl.DC_COHSTA to be cleared
check_cm_disabled:
	csrr t1, 0x7ca
	srli t1, t1, 20
	andi t1, t1, 0x1
	bnez t1, check_cm_disabled

	ret

############################################
# void __ae350_enable_clk(void)           #
############################################
	.section .text, "ax", %progbits
	.align 3
	.global __ae350_enable_clk
__ae350_enable_clk:
	/* 1.1 Enable D-cache coherency */
	li		t1, 0x80000
	csrs	0x7ca, t1
	/* 1.2 Wait for mcache_ctl.DC_COHSTA to be set */
check_cm_enabled:
	csrr	t1, 0x7ca
	srli	t1, t1, 20
	andi	t1, t1, 0x1
	beqz	t1, check_cm_enabled
	/* 1.3 enable I/D-cache */
	csrs	0x7ca, 0x3

	ret

	.section .text, "ax", %progbits
	.align 3
	.global __ae350_enable_clk_warmboot
__ae350_enable_clk_warmboot:
	call ra, __ae350_enable_clk
	j _start_warm
