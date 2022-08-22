/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Andes Technology Corporation
 *
 * Authors:
 *   Zong Li <zong@andestech.com>
 *   Nylon Chen <nylon7@andestech.com>
 *   Yu Chien Peter Lin <peterlin@andestech.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_timer.h>

#ifndef __TIMER_ANDES_PLMT_H__
#define __TIMER_ANDES_PLMT_H__

#define DEFAULT_AE350_PLMT_FREQ 60000000

struct plmt_data {
	u32 hart_count;
	unsigned long size;
	volatile u64 *time_val;
	volatile u64 *time_cmp;
};

u64 plmt_timer_value(void);
void plmt_timer_event_stop(void);
void plmt_timer_event_start(u64 next_event);

#endif /* __TIMER_ANDES_PLMT_H__ */
