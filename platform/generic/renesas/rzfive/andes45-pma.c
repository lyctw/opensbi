// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Renesas Electronics Corp.
 *
 * Copyright (c) 2020 Andes Technology Corporation
 *
 * Authors:
 *      Nick Hu <nickhu@andestech.com>
 *      Nylon Chen <nylon7@andestech.com>
 */

#include <andes45-pma.h>
#include <libfdt.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_error.h>
#include <sbi_utils/fdt/fdt_helper.h>

/* Configuration Registers */
#define ANDES45_CSR_MMSC_CFG		0xFC2
#define ANDES45_CSR_MMSC_PPMA_OFFSET	(1 << 30)

#define ANDES45_PMAADDR_0		0xBD0
#define ANDES45_PMAADDR_1		0xBD1
#define ANDES45_PMAADDR_2		0xBD2
#define ANDES45_PMAADDR_3		0xBD3
#define ANDES45_PMAADDR_4		0xBD4
#define ANDES45_PMAADDR_5		0xBD5
#define ANDES45_PMAADDR_6		0xBD6
#define ANDES45_PMAADDR_7		0xBD7
#define ANDES45_PMAADDR_8		0xBD8
#define ANDES45_PMAADDR_9		0xBD9
#define ANDES45_PMAADDR_10		0xBDA
#define ANDES45_PMAADDR_11		0xBDB
#define ANDES45_PMAADDR_12		0xBDC
#define ANDES45_PMAADDR_13		0xBDD
#define ANDES45_PMAADDR_14		0xBDE
#define ANDES45_PMAADDR_15		0xBDF

/* n = 0 - 3 */
#define ANDES45_PMACFG_n(n)		(0xBC0 + (n))

static inline unsigned long andes45_pma_read_cfg(unsigned int i)
{
	unsigned long val = 0;

	if (!i)
		val = csr_read(ANDES45_PMACFG_n(0));
	else if (i == 1)
		val = csr_read(ANDES45_PMACFG_n(2));

	return val;
}

static inline void andes45_pma_write_cfg(unsigned int i, unsigned long val)
{
	if (!i)
		csr_write(ANDES45_PMACFG_n(0), val);
	else if (i == 1)
		csr_write(ANDES45_PMACFG_n(2), val);
}

static inline void andes45_pma_write_addr(unsigned int i, unsigned long val)
{
	if (i == 0)
		csr_write(ANDES45_PMAADDR_0, val);
	else if (i == 1)
		csr_write(ANDES45_PMAADDR_1, val);
	else if (i == 2)
		csr_write(ANDES45_PMAADDR_2, val);
	else if (i == 3)
		csr_write(ANDES45_PMAADDR_3, val);
	else if (i == 4)
		csr_write(ANDES45_PMAADDR_4, val);
	else if (i == 5)
		csr_write(ANDES45_PMAADDR_5, val);
	else if (i == 6)
		csr_write(ANDES45_PMAADDR_6, val);
	else if (i == 7)
		csr_write(ANDES45_PMAADDR_7, val);
	else if (i == 8)
		csr_write(ANDES45_PMAADDR_8, val);
	else if (i == 9)
		csr_write(ANDES45_PMAADDR_9, val);
	else if (i == 10)
		csr_write(ANDES45_PMAADDR_10, val);
	else if (i == 11)
		csr_write(ANDES45_PMAADDR_11, val);
	else if (i == 12)
		csr_write(ANDES45_PMAADDR_12, val);
	else if (i == 13)
		csr_write(ANDES45_PMAADDR_13, val);
	else if (i == 14)
		csr_write(ANDES45_PMAADDR_14, val);
	else if (i == 15)
		csr_write(ANDES45_PMAADDR_15, val);
}

static inline unsigned long andes45_pma_read_addr(unsigned int i)
{
	unsigned long ret = 0;

	if (i == 0)
		ret = csr_read(ANDES45_PMAADDR_0);
	else if (i == 1)
		ret = csr_read(ANDES45_PMAADDR_1);
	else if (i == 2)
		ret = csr_read(ANDES45_PMAADDR_2);
	else if (i == 3)
		ret = csr_read(ANDES45_PMAADDR_3);
	else if (i == 4)
		ret = csr_read(ANDES45_PMAADDR_4);
	else if (i == 5)
		ret = csr_read(ANDES45_PMAADDR_5);
	else if (i == 6)
		ret =  csr_read(ANDES45_PMAADDR_6);
	else if (i == 7)
		ret = csr_read(ANDES45_PMAADDR_7);
	else if (i == 8)
		ret = csr_read(ANDES45_PMAADDR_8);
	else if (i == 9)
		ret = csr_read(ANDES45_PMAADDR_9);
	else if (i == 10)
		ret = csr_read(ANDES45_PMAADDR_10);
	else if (i == 11)
		ret = csr_read(ANDES45_PMAADDR_11);
	else if (i == 12)
		ret = csr_read(ANDES45_PMAADDR_12);
	else if (i == 13)
		ret = csr_read(ANDES45_PMAADDR_13);
	else if (i == 14)
		ret = csr_read(ANDES45_PMAADDR_14);
	else if (i == 15)
		ret = csr_read(ANDES45_PMAADDR_15);

	return ret;
}

static unsigned long andes45_pma_setup(unsigned long addr,
				       unsigned long size,
				       unsigned int entry_id,
				       u32 flag)
{
	unsigned long size_tmp, shift, pmacfg_val;
	unsigned long pmaaddr;
	unsigned int power;
	char *pmaxcfg;

	if (size < (1 << 12))
		return SBI_EINVAL;

	if (flag > 0xff || entry_id > 15)
		return SBI_EINVAL;

	if (!(flag & ANDES45_PMACFG_ETYP_NAPOT))
		return SBI_EINVAL;

	if ((addr & (size - 1)) != 0)
		return SBI_EINVAL;

	/* Calculate the NAPOT table for pmaaddr */
	size_tmp = size;
	shift = 0;
	power = 0;
	while (size_tmp != 0x1) {
		size_tmp = size_tmp >> 1;
		power++;
		if (power > 3)
			shift = (shift << 1) | 0x1;
	}

	pmacfg_val = andes45_pma_read_cfg(entry_id / 8);
	pmaxcfg = (char *)&pmacfg_val + (entry_id % 8);
	*pmaxcfg = 0;
	*pmaxcfg = (u8)flag;

	andes45_pma_write_cfg(entry_id / 8, pmacfg_val);

	pmaaddr = (addr >> 2) + (size >> 3) - 1;

	andes45_pma_write_addr(entry_id, pmaaddr);

	return andes45_pma_read_addr(entry_id) == pmaaddr ? pmaaddr : SBI_EINVAL;
}

static int andes45_fdt_pma_resv(void *fdt, const struct andes45_pma_region *pma,
				unsigned int index, int parent)
{
	int na = fdt_address_cells(fdt, 0);
	int ns = fdt_size_cells(fdt, 0);
	static bool dma_default = false;
	fdt32_t addr_high, addr_low;
	fdt32_t size_high, size_low;
	int subnode, err;
	fdt32_t reg[4];
	fdt32_t *val;
	char name[32];

	addr_high = (u64)pma->pa >> 32;
	addr_low = pma->pa;
	size_high = (u64)pma->size >> 32;
	size_low = pma->size;

	if (na > 1 && addr_high)
		sbi_snprintf(name, sizeof(name),
			     "pma_resv%d@%x,%x", index,
			     addr_high, addr_low);
	else
		sbi_snprintf(name, sizeof(name),
			     "pma_resv%d@%x", index,
			     addr_low);

	subnode = fdt_add_subnode(fdt, parent, name);
	if (subnode < 0)
		return subnode;

	if (pma->shared_dma) {
		err = fdt_setprop_string(fdt, subnode, "compatible", "shared-dma-pool");
		if (err < 0)
			return err;
	}

	if (pma->no_map) {
		err = fdt_setprop_empty(fdt, subnode, "no-map");
		if (err < 0)
			return err;
	}

	/* Linux allows single linux,dma-default region. */
	if (pma->dma_default) {
		if (dma_default)
			return SBI_EINVAL;

		err = fdt_setprop_empty(fdt, subnode, "linux,dma-default");
		if (err < 0)
			return err;
		dma_default = true;
	}

	/* encode the <reg> property value */
	val = reg;
	if (na > 1)
		*val++ = cpu_to_fdt32(addr_high);
	*val++ = cpu_to_fdt32(addr_low);
	if (ns > 1)
		*val++ = cpu_to_fdt32(size_high);
	*val++ = cpu_to_fdt32(size_low);

	err = fdt_setprop(fdt, subnode, "reg", reg,
			  (na + ns) * sizeof(fdt32_t));
	if (err < 0)
		return err;

	return 0;
}

static int andes45_fdt_reserved_memory_fixup(void *fdt,
					     const struct andes45_pma_region *pma,
					     unsigned int entry)
{
	int parent;

	/* try to locate the reserved memory node */
	parent = fdt_path_offset(fdt, "/reserved-memory");
	if (parent < 0) {
		int na = fdt_address_cells(fdt, 0);
		int ns = fdt_size_cells(fdt, 0);
		int err;

		/* if such node does not exist, create one */
		parent = fdt_add_subnode(fdt, 0, "reserved-memory");
		if (parent < 0)
			return parent;

		err = fdt_setprop_empty(fdt, parent, "ranges");
		if (err < 0)
			return err;

		err = fdt_setprop_u32(fdt, parent, "#size-cells", ns);
		if (err < 0)
			return err;

		err = fdt_setprop_u32(fdt, parent, "#address-cells", na);
		if (err < 0)
			return err;
	}

	return andes45_fdt_pma_resv(fdt, pma, entry, parent);
}

int andes45_pma_setup_regions(const struct andes45_pma_region *pma_regions,
			      unsigned int pma_regions_count)
{
	unsigned long mmsc = csr_read(ANDES45_CSR_MMSC_CFG);
	unsigned int dt_populate_cnt;
	unsigned int i, j;
	unsigned long pa;
	void *fdt;
	int ret;

	if (!pma_regions || !pma_regions_count)
		return 0;

	if (pma_regions_count > ANDES45_MAX_PMA_REGIONS)
		return SBI_EINVAL;

	if ((mmsc & ANDES45_CSR_MMSC_PPMA_OFFSET) == 0)
		return SBI_ENOTSUPP;

	/* Configure the PMA regions */
	for (i = 0; i < pma_regions_count; i++) {
		pa = andes45_pma_setup(pma_regions[i].pa,
				       pma_regions[i].size,
				       i, pma_regions[i].flags);
		if (pa == SBI_EINVAL)
			return SBI_EINVAL;
	}

	dt_populate_cnt = 0;
	for (i = 0; i < pma_regions_count; i++) {
		if (!pma_regions[i].dt_populate)
			continue;
		dt_populate_cnt++;
	}

	if (!dt_populate_cnt)
		return 0;

	fdt = fdt_get_address();

	ret = fdt_open_into(fdt, fdt, fdt_totalsize(fdt) + (64 * dt_populate_cnt));
	if (ret < 0)
		return ret;

	for (i = 0, j = 0; i < pma_regions_count; i++) {
		if (!pma_regions[i].dt_populate)
			continue;

		ret = andes45_fdt_reserved_memory_fixup(fdt, &pma_regions[i], j++);
		if (ret)
			return ret;
	}

	return 0;
}
