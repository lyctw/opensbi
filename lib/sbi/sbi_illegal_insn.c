/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_barrier.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_emulate_csr.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_illegal_insn.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_unpriv.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_string.h>

typedef int (*illegal_insn_func)(ulong insn, struct sbi_trap_regs *regs);

struct page_stat {
    /* Number of user entries */
    unsigned long user_entry_count;
    /* Number of kernel entries */
    unsigned long kernel_entry_count;
};

const char* level2pagetype(int level) {
	switch (level) {
	case 4:
		return "256TiB petapage";
	case 3:
		return "512GiB terapage";
	case 2:
		return "1GiB gigapage";
	case 1:
		return "2MiB megapage";
	case 0:
		return "4KiB page";
	default:
		return "Invalid page";
	}
}

const unsigned long level2pagesize(int level)
{
	switch (level) {
	case 4:
		sbi_panic("256TiB petapage...\n");
		return 0;
	case 3:
		sbi_panic("512GiB petapage...\n");
		return 0;
	case 2:
		return (1 << 30); // 1GiB
	case 1:
		return (2 << 20); // 2MiB
	case 0:
		return (4 << 10); // 4KiB
	default:
		sbi_panic("Invalid page size\n");
		return 0;
	}
}

const char* sizeToReadableUnit(unsigned long size) {
	static char readableSize[20]; // Buffer to hold the result string
	const char* units[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB" };
	size_t unitIndex = 0;

	// Iterate through units array to find the appropriate unit
	while (size >= 1024 && unitIndex < (sizeof(units) / sizeof(units[0]) - 1)) {
		size /= 1024;
		unitIndex++;
	}

	// Format the final readable size string
	sbi_snprintf(readableSize, sizeof(readableSize), "%ld%s", size, units[unitIndex]);
	return readableSize;
}

static const char* pte2perm(pte_t pte)
{
	static char perm[9];
	sbi_strcpy(perm, "DAGUXWRV");
	for (int i = 7; i >= 0; i--) {
		if (pte & BIT(i))
			continue;
		perm[7 - i] = '.';
	}
	return perm;
}

void rv64_va_extend_msb(uint64_t *va) {
	/* TODO: Check non-canonical va addresses here
	 * To allows an OS to use most-significant bits of a full-size (64-bit)
	 * virtual address to quickly distinguish user and supervisor address
	 * regions, Sv39/Sv48/Sv57 follow "sign extension"-like addresses. */
	ulong satp = csr_read(CSR_SATP);
	int mode = satp >> 60;

	switch (mode) {
	case 8:
		if (*va & BIT(38)) // Extend bit 39-63 if the 39th bit is set
			*va |= GENMASK(63, 39);
		return;
	case 9:
		if (*va & BIT(47)) // Extend bit 48-63 if the 48th bit is set
			*va |= GENMASK(63, 48);
		return;
	case 10:
		if (*va & BIT(56)) // Extend bit 57-63 if the 57th bit is set
			*va |= GENMASK(63, 57);
		return;
	default:
		sbi_panic("Invalid satp.MODE\n");
	}
}

static void ptdump(pagetable_t pagetable, unsigned int level,
		   virtual_addr_t va, struct page_stat* pgstat)
{
	int entry_num = 1 << VPN_WIDTH;
	for (int i = 0; i < entry_num; i++) {
		pte_t pte = pagetable[i];
		if ((pte & PTE_V) == 0)
			continue;

		if (pte & PTE_U)
			pgstat->user_entry_count++;
		else
			pgstat->kernel_entry_count++;
		bool is_leaf = !!(pte & (PTE_R|PTE_W|PTE_X));

		/* Sanity check */
		if (!is_leaf && (level == 0))
			sbi_panic("The last level valid PTE must be a leaf\n");

		/* Indentation */
		for(int j = 0; j < level; j++)
			sbi_printf("    ");

		if (!is_leaf) {
			// this PTE points to a next-level page table.
			sbi_printf("[L%d/%d@%#lx] perm: %s | next table addr: %#lx\n",
				   level, i, (ulong)&pagetable[i], pte2perm(pte),
				   PTE2PA(pte));
			physical_addr_t next_tbl = PTE2PA(pte);
			ptdump((pagetable_t)next_tbl, level - 1,
				INSERT_FIELD(va, VPN_MASK << (level * VPN_WIDTH + PAGE_SHIFT), i),
				pgstat);
		} else {
			virtual_addr_t curr_va = INSERT_FIELD(va, VPN_MASK << (level * VPN_WIDTH + PAGE_SHIFT), i);
			rv64_va_extend_msb(&curr_va);
			sbi_printf("[L%d/%d@%#lx] perm: %s | va: %#lx - %#lx (%s) | pa %#lx\n",
				   level, i, (ulong)&pagetable[i], pte2perm(pte),
				   curr_va,
				   curr_va + level2pagesize(level) - 1,
				   level2pagetype(level),
				   PTE2PA(pte));
		}
	}
}

static int ptdump_handler(ulong insn, struct sbi_trap_regs *regs)
{
#if __riscv_xlen == 64
	ulong satp = csr_read(CSR_SATP);
	int mode = satp >> 60;
	int max_level = mode - 6;
	/* On RV64, each level has 512 entries */
	uint64_t max_entries = 1;
	struct page_stat pgstat = { 0 };
	unsigned long total_entry_count = 0;

	for (int i = 0; i <= max_level; i++)
		max_entries *= 512ull;

	if (mode < 8 || mode > 10) // Sv39 (8) / Sv47 (9) / Sv57 (10)
		sbi_panic("Paging mode is disabled/unsupported\n");

	pagetable_t pagetable = (pagetable_t)((satp & GENMASK(43, 0)) << PAGE_SHIFT);
	sbi_printf("=================[ ptdump start ]=================================\n");
	ptdump(pagetable, max_level, 0, &pgstat);
	total_entry_count = pgstat.user_entry_count + pgstat.kernel_entry_count;
	sbi_printf("hart%d, satp: %#lx (MODE: Sv%s, ASID: %lu)\n",
		   current_hartid(),
		   satp,
		   (mode == 8) ? "39" :
		    ((mode == 9) ? "48" :
		     ((mode == 10) ? "57" : "??")),
		    EXTRACT_FIELD(satp, GENMASK(59, 44)));
	sbi_printf("entry usage: user: %lu, kernel: %lu, total: %lu (%lu%%, %lu/%lu)\n",
		   pgstat.user_entry_count,
		   pgstat.kernel_entry_count,
		   total_entry_count,
		   total_entry_count / max_entries,
		   total_entry_count, max_entries);
	sbi_printf("=================[ ptdump end ]========================\n");
#else
	/* RV32 is not supported yet */
#endif
	regs->mepc += 4;
	return 0;
}

static int truly_illegal_insn(ulong insn, struct sbi_trap_regs *regs)
{
	struct sbi_trap_info trap;

	trap.cause = CAUSE_ILLEGAL_INSTRUCTION;
	trap.tval = insn;
	trap.tval2 = 0;
	trap.tinst = 0;
	trap.gva   = 0;

	return sbi_trap_redirect(regs, &trap);
}

static int misc_mem_opcode_insn(ulong insn, struct sbi_trap_regs *regs)
{
	/* Errata workaround: emulate `fence.tso` as `fence rw, rw`. */
	if ((insn & INSN_MASK_FENCE_TSO) == INSN_MATCH_FENCE_TSO) {
		smp_mb();
		regs->mepc += 4;
		return 0;
	}

	return truly_illegal_insn(insn, regs);
}

static int system_opcode_insn(ulong insn, struct sbi_trap_regs *regs)
{
	int do_write, rs1_num = (insn >> 15) & 0x1f;
	ulong rs1_val = GET_RS1(insn, regs);
	int csr_num   = (u32)insn >> 20;
	ulong prev_mode = (regs->mstatus & MSTATUS_MPP) >> MSTATUS_MPP_SHIFT;
	ulong csr_val, new_csr_val;

	if (prev_mode == PRV_M) {
		sbi_printf("%s: Failed to access CSR %#x from M-mode",
			__func__, csr_num);
		return SBI_EFAIL;
	}

	/* TODO: Ensure that we got CSR read/write instruction */

	if (sbi_emulate_csr_read(csr_num, regs, &csr_val))
		return truly_illegal_insn(insn, regs);

	do_write = rs1_num;
	switch (GET_RM(insn)) {
	case 1:
		new_csr_val = rs1_val;
		do_write    = 1;
		break;
	case 2:
		new_csr_val = csr_val | rs1_val;
		break;
	case 3:
		new_csr_val = csr_val & ~rs1_val;
		break;
	case 5:
		new_csr_val = rs1_num;
		do_write    = 1;
		break;
	case 6:
		new_csr_val = csr_val | rs1_num;
		break;
	case 7:
		new_csr_val = csr_val & ~rs1_num;
		break;
	default:
		return truly_illegal_insn(insn, regs);
	}

	if (do_write && sbi_emulate_csr_write(csr_num, regs, new_csr_val))
		return truly_illegal_insn(insn, regs);

	SET_RD(insn, regs, csr_val);

	regs->mepc += 4;

	return 0;
}

static const illegal_insn_func illegal_insn_table[32] = {
	truly_illegal_insn, /* 0 */
	truly_illegal_insn, /* 1 */
	truly_illegal_insn, /* 2 */
	misc_mem_opcode_insn, /* 3 */
	truly_illegal_insn, /* 4 */
	truly_illegal_insn, /* 5 */
	truly_illegal_insn, /* 6 */
	truly_illegal_insn, /* 7 */
	truly_illegal_insn, /* 8 */
	truly_illegal_insn, /* 9 */
	truly_illegal_insn, /* 10 */
	truly_illegal_insn, /* 11 */
	/*
	 * Insert the illegal instruction below at any program/OS kernel code
	 * to trigger fw page table dump
	 * asm volatile(".word 0x12346533\t\n");
	 */
	ptdump_handler, /* 12 */
	truly_illegal_insn, /* 13 */
	truly_illegal_insn, /* 14 */
	truly_illegal_insn, /* 15 */
	truly_illegal_insn, /* 16 */
	truly_illegal_insn, /* 17 */
	truly_illegal_insn, /* 18 */
	truly_illegal_insn, /* 19 */
	truly_illegal_insn, /* 20 */
	truly_illegal_insn, /* 21 */
	truly_illegal_insn, /* 22 */
	truly_illegal_insn, /* 23 */
	truly_illegal_insn, /* 24 */
	truly_illegal_insn, /* 25 */
	truly_illegal_insn, /* 26 */
	truly_illegal_insn, /* 27 */
	system_opcode_insn, /* 28 */
	truly_illegal_insn, /* 29 */
	truly_illegal_insn, /* 30 */
	truly_illegal_insn  /* 31 */
};

int sbi_illegal_insn_handler(struct sbi_trap_context *tcntx)
{
	struct sbi_trap_regs *regs = &tcntx->regs;
	ulong insn = tcntx->trap.tval;
	struct sbi_trap_info uptrap;

	/*
	 * We only deal with 32-bit (or longer) illegal instructions. If we
	 * see instruction is zero OR instruction is 16-bit then we fetch and
	 * check the instruction encoding using unprivilege access.
	 *
	 * The program counter (PC) in RISC-V world is always 2-byte aligned
	 * so handling only 32-bit (or longer) illegal instructions also help
	 * the case where MTVAL CSR contains instruction address for illegal
	 * instruction trap.
	 */

	sbi_pmu_ctr_incr_fw(SBI_PMU_FW_ILLEGAL_INSN);
	if (unlikely((insn & 3) != 3)) {
		insn = sbi_get_insn(regs->mepc, &uptrap);
		if (uptrap.cause)
			return sbi_trap_redirect(regs, &uptrap);
		if ((insn & 3) != 3)
			return truly_illegal_insn(insn, regs);
	}

	return illegal_insn_table[(insn & 0x7c) >> 2](insn, regs);
}
