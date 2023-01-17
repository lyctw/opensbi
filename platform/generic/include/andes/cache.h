#ifndef _RISCV_ANDES_CACHE_H
#define _RISCV_ANDES_CACHE_H

static __always_inline int mcall_icache_op(bool enable)
{
	if (enable)
		csr_set(CSR_MCACHE_CTL, MCACHE_CTL_IC_EN);
	else {
		csr_clear(CSR_MCACHE_CTL, MCACHE_CTL_IC_EN);
		RISCV_FENCE_I;
	}
	return 0;
}

//static __always_inline int mcall_dcache_op(bool enable)
//{
//	if (enable) {
//		csr_set(CSR_MCACHE_CTL, MCACHE_CTL_DC_EN);
//	} else {
//		csr_clear(CSR_MCACHE_CTL, MCACHE_CTL_DC_EN);
//		csr_write(CSR_MCCTLCOMMAND, CCTLCOMMAND_L1D_WBINVAL_ALL);
//	}
//	return 0;
//}

static __always_inline int mcall_dcache_op(bool enable)
{
	/*
	 * Note, this function should NOT be called in programming sequence of
	 * SMU sleep mode, as the i-cache should be disable along with d-cache
	 */
	if (enable) {
		if (is_andes45()) {
			/* enable d-cache coherency */
			csr_set(CSR_MCACHE_CTL, MCACHE_CTL_DC_COHEN_EN);

			/*
			 * If mcache_ctl.DC_COHEN cannot be written,
			 * we assume this platform does not support
			 * Coherence Manager (CM).
			 */
			if (EXTRACT_FIELD(CSR_MCACHE_CTL, MCACHE_CTL_DC_COHEN_EN)) {
				/* Wait for mcache_ctl.DC_COHSTA (read-only) bit to be set */
				while (!EXTRACT_FIELD(CSR_MCACHE_CTL,MCACHE_CTL_DC_COHSTA));
			}
		}

		csr_write(CSR_MCCTLCOMMAND, CCTLCOMMAND_L1D_INVAL_ALL);
		csr_set(CSR_MCACHE_CTL, MCACHE_CTL_DC_EN);
	}
	else {
		csr_clear(CSR_MCACHE_CTL, MCACHE_CTL_DC_EN);
		csr_write(CSR_MCCTLCOMMAND, CCTLCOMMAND_L1D_WBINVAL_ALL);

		if (is_andes45()) {
			csr_clear(CSR_MCACHE_CTL, MCACHE_CTL_DC_COHEN_EN);

			/*
			 * If andes45 not support CM, mcache_ctl.DC_COHSTA
			 * is hard-wired to 0
			 */
			while (EXTRACT_FIELD(CSR_MCACHE_CTL, MCACHE_CTL_DC_COHSTA));
		}
	}
	return 0;
}

static __always_inline void mcall_dcache_wbinval_all(void)
{
	csr_write(CSR_MCCTLCOMMAND, CCTLCOMMAND_L1D_WBINVAL_ALL);
}

#endif /* _RISCV_ANDES_CACHE_H */
