/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Andes Technology Corporation
 *
 * Authors:
 *   Zong Li <zong@andestech.com>
 *   Nylon Chen <nylon7@andestech.com>
 */

#ifndef _AE350_PLATFORM_H_
#define _AE350_PLATFORM_H_

#include "common-platform.h"

#define AE350_L2C_ADDR			0xe0500000

enum sbi_ext_andes_fid {
	SBI_EXT_ANDES_GET_MCACHE_CTL_STATUS = 0,
	SBI_EXT_ANDES_GET_MMISC_CTL_STATUS,
	SBI_EXT_ANDES_SET_MCACHE_CTL,
	SBI_EXT_ANDES_SET_MMISC_CTL,
	SBI_EXT_ANDES_ICACHE_OP,
	SBI_EXT_ANDES_DCACHE_OP,
	SBI_EXT_ANDES_L1CACHE_I_PREFETCH,
	SBI_EXT_ANDES_L1CACHE_D_PREFETCH,
	SBI_EXT_ANDES_NON_BLOCKING_LOAD_STORE,
	SBI_EXT_ANDES_WRITE_AROUND,
};

#endif /* _AE350_PLATFORM_H_ */
