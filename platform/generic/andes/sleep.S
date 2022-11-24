#include <sbi/riscv_encoding.h>
#include <sbi/sbi_scratch.h>
#include <atcsmu.h>

###################################
# void ae350_disable_clocks(void) #
###################################
	.section .text, "ax", %progbits
	.align 3
	.global ae350_disable_clocks
ae350_disable_clocks:
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
# void ae350_enable_clocks(void)           #
############################################
	.section .text, "ax", %progbits
	.align 3
	.global ae350_enable_clocks
ae350_enable_clocks:
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

	##XDDDDDDDDDDDDDDDDDDDDDDDDd
	#la t1, smu
	#REG_L t1, 8(t1) # smu.sleep_mode

	/* Jump to warmboot */ // PASS
	csrr	t1, CSR_MSCRATCH
	jalr	x0, SBI_SCRATCH_WARMBOOT_ADDR_OFFSET(t1)

	/* Jump to warmboot */ // FAILED
	#csrr	t1, CSR_MSCRATCH
	#REG_L	t1, SBI_SCRATCH_WARMBOOT_ADDR_OFFSET(t1)
	#jalr	x0, 0(t1)

	// PASS
	#j _start_warm

done:
	/* light sleep will resume from wfi() */
	ret
