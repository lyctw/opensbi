#include <sbi_utils/timer/andes_plmt.h>

struct plmt_data plmt;

u64 plmt_timer_value(void)
{
#if __riscv_xlen == 64
	return readq_relaxed(plmt.time_val);
#else
	u32 lo, hi;

	do {
		hi = readl_relaxed((void *)plmt.time_val + 0x04);
		lo = readl_relaxed(plmt.time_val);
	} while (hi != readl_relaxed((void *)plmt.time_val + 0x04));

	return ((u64)hi << 32) | (u64)lo;
#endif
}

void plmt_timer_event_stop(void)
{
	u32 target_hart = current_hartid();

	if (plmt.hart_count <= target_hart)
		ebreak();

	/* Clear PLMT Time Compare */
#if __riscv_xlen == 64
	writeq_relaxed(-1ULL, &plmt.time_cmp[target_hart]);
#else
	writel_relaxed(-1UL, &plmt.time_cmp[target_hart]);
	writel_relaxed(-1UL, (void *)(&plmt.time_cmp[target_hart]) + 0x04);
#endif
}

void plmt_timer_event_start(u64 next_event)
{
	u32 target_hart = current_hartid();

	if (plmt.hart_count <= target_hart)
		ebreak();

	/* Program PLMT Time Compare */
#if __riscv_xlen == 64
	writeq_relaxed(next_event, &plmt.time_cmp[target_hart]);
#else
	u32 mask = -1UL;

	writel_relaxed(next_event & mask, &plmt.time_cmp[target_hart]);
	writel_relaxed(next_event >> 32,
		       (void *)(&plmt.time_cmp[target_hart]) + 0x04);
#endif
}
