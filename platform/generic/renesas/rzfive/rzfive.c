// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Renesas Electronics Corp.
 *
 */

#include <andes45-pma.h>
#include <platform_override.h>
#include <sbi/sbi_domain.h>
#include <sbi_utils/fdt/fdt_helper.h>

#define RENESAS_RZFIVE_SBI_EXT_IOCP_SW_WORKAROUND	0

/* AX45MP registers */
#define AX45MP_CSR_MISA_CFG			0x301
#define AX45MP_CSR_MICM_CFG			0xfc0
#define AX45MP_CSR_MDCM_CFG			0xfc1
#define AX45MP_CSR_MMSC_CFG			0xfc2
#define AX45MP_CSR_MCACHE_CTL			0x7ca

/* AX45MP register bit offsets and masks */
#define AX45MP_MISA_20_OFFSET			20
#define AX45MP_MISA_20_MASK			(0x1 << AX45MP_MISA_20_OFFSET)

#define AX45MP_MICM_CFG_ISZ_OFFSET		6
#define AX45MP_MICM_CFG_ISZ_MASK		(0x7  << AX45MP_MICM_CFG_ISZ_OFFSET)

#define AX45MP_MDCM_CFG_DSZ_OFFSET		6
#define AX45MP_MDCM_CFG_DSZ_MASK		(0x7  << AX45MP_MDCM_CFG_DSZ_OFFSET)

#define AX45MP_MMSC_CFG_CCTLCSR_OFFSET		16
#define AX45MP_MMSC_CFG_CCTLCSR_MASK		(0x1 << AX45MP_MMSC_CFG_CCTLCSR_OFFSET)
#define AX45MP_MMSC_IOCP_OFFSET			47
#define AX45MP_MMSC_IOCP_MASK			(0x1ULL << AX45MP_MMSC_IOCP_OFFSET)

#define AX45MP_MCACHE_CTL_CCTL_SUEN_OFFSET	8
#define AX45MP_MCACHE_CTL_CCTL_SUEN_MASK	(0x1 << AX45MP_MCACHE_CTL_CCTL_SUEN_OFFSET)

static const struct andes45_pma_region renesas_rzfive_pma_regions[] = {
	{
		.pa = 0x58000000,
		.size = 0x8000000,
		.flags = ANDES45_PMACFG_ETYP_NAPOT |
			 ANDES45_PMACFG_MTYP_MEM_NON_CACHE_BUF,
		.dt_populate = true,
		.shared_dma = true,
		.no_map = true,
		.dma_default = true,
	},
};

static int renesas_rzfive_final_init(bool cold_boot, const struct fdt_match *match)
{
	return andes45_pma_setup_regions(renesas_rzfive_pma_regions,
					 array_size(renesas_rzfive_pma_regions));
}

static bool renesas_rzfive_cpu_cache_controlable(void)
{
	return (((csr_read(AX45MP_CSR_MICM_CFG) & AX45MP_MICM_CFG_ISZ_MASK) ||
		 (csr_read(AX45MP_CSR_MDCM_CFG) & AX45MP_MDCM_CFG_DSZ_MASK)) &&
		(csr_read(AX45MP_CSR_MISA_CFG) & AX45MP_MISA_20_MASK) &&
		(csr_read(AX45MP_CSR_MMSC_CFG) & AX45MP_MMSC_CFG_CCTLCSR_MASK) &&
		(csr_read(AX45MP_CSR_MCACHE_CTL) & AX45MP_MCACHE_CTL_CCTL_SUEN_MASK));
}

static bool renesas_rzfive_cpu_iocp_disabled(void)
{
	return (csr_read(AX45MP_CSR_MMSC_CFG) & AX45MP_MMSC_IOCP_MASK) ? false : true;
}

static bool renesas_rzfive_apply_iocp_sw_workaround(void)
{
	return renesas_rzfive_cpu_cache_controlable() & renesas_rzfive_cpu_iocp_disabled();
}
static int renesas_rzfive_vendor_ext_provider(long extid, long funcid,
					      const struct sbi_trap_regs *regs,
					      unsigned long *out_value,
					      struct sbi_trap_info *out_trap,
					      const struct fdt_match *match)
{
	switch (funcid) {
	case RENESAS_RZFIVE_SBI_EXT_IOCP_SW_WORKAROUND:
		*out_value = renesas_rzfive_apply_iocp_sw_workaround();
		break;

	default:
		break;
	}

	return 0;
}
int renesas_rzfive_early_init(bool cold_boot, const struct fdt_match *match)
{
	/*
	 * Renesas RZ/Five RISC-V SoC has Instruction local memory and
	 * Data local memory (ILM & DLM) mapped between region 0x30000
	 * to 0x4FFFF. When a virtual address falls within this range,
	 * the MMU doesn't trigger a page fault; it assumes the virtual
	 * address is a physical address which can cause undesired
	 * behaviours for statically linked applications/libraries. To
	 * avoid this, add the ILM/DLM memory regions to the root domain
	 * region of the PMPU with permissions set to 0x0 so that any
	 * access to these regions gets blocked.
	 */
	return 0; //sbi_domain_root_add_memrange(0x30000, 0x20000, 0x1000, 0x0);
}

static const struct fdt_match renesas_rzfive_match[] = {
	{ .compatible = "renesas,r9a07g043f01" },
	{ /* sentinel */ }
};

const struct platform_override renesas_rzfive = {
	.match_table = renesas_rzfive_match,
	.early_init = renesas_rzfive_early_init,
	.final_init = renesas_rzfive_final_init,
	.vendor_ext_provider = renesas_rzfive_vendor_ext_provider,
};
