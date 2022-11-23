#include <sbi/riscv_encoding.h>
#include <sbi/sbi_scratch.h>

	.section .text, "ax", %progbits
	.align 3
	.global cpu_dcache_enable
cpu_dcache_enable:
	/* Enable D-cache coherency */
	li		t1, 0x80000
	csrs	0x7ca, t1
	/* 1.2 Wait for mcache_ctl.DC_COHSTA to be set */
wait_for_cm:
	csrr	t1, 0x7ca
	srli	t1, t1, 12
	li		t5, 0x100
	and		t1, t1, t5
	beqz	t1, wait_for_cm
	/* enable D-cache */
	csrs	0x7ca, 0x2

yc_test:
	/* Jump to warmboot */
	csrr	t1, CSR_MSCRATCH
	jalr	x0, SBI_SCRATCH_WARMBOOT_ADDR_OFFSET(t1)

	/* Jump to warmboot */
	#csrr	t1, CSR_MSCRATCH
	#REG_L	t1, SBI_SCRATCH_WARMBOOT_ADDR_OFFSET(t1)
	#jalr	x0, 0(t1)
