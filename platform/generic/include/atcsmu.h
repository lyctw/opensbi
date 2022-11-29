#ifndef _RISCV_ATCSMU_H
#define _RISCV_ATCSMU_H

#define PCS0_WE_OFF     0x90
#define PCS0_CTL_OFF    0x94
#define PCS0_CFG_OFF    0x80
#define PCSm_WE_OFF(i)  ((i + 3) * 0x20 + PCS0_WE_OFF)
#define PCSm_CTL_OFF(i) ((i + 3) * 0x20 + PCS0_CTL_OFF)
#define PCSm_CFG_OFF(i) ((i + 3) * 0x20 + PCS0_CFG_OFF)
#define SLEEP_CMD       0x3
#define WAKEUP_CMD      0x8
#define LIGHTSLEEP_MODE  0
#define DEEPSLEEP_MODE   1
#define LIGHT_SLEEP_CMD (SLEEP_CMD | (LIGHTSLEEP_MODE << PCS_CTL_PARAM_OFF))
#define DEEP_SLEEP_CMD (SLEEP_CMD | (DEEPSLEEP_MODE << PCS_CTL_PARAM_OFF))
#define PCS_CTL_PARAM_OFF 0x3
#define PCS_CTL_PARAM_BITS 5
#define PCS_CTL_PARAM_MASK ((1UL << PCS_CTL_PARAM_BITS) - 1)
#define PCS_CFG_LIGHT_SLEEP_OFF 2
#define PCS_CFG_DEEP_SLEEP_OFF 3
#define SMU_RESET_VEC_LO_OFF 0x50
#define SMU_RESET_VEC_HI_OFF 0x60
#define SMU_HARTn_RESET_VEC_LO(n) (SMU_RESET_VEC_LO_OFF + (n * 0x4))
#define SMU_HARTn_RESET_VEC_HI(n) (SMU_RESET_VEC_HI_OFF + (n * 0x4))
#define PCS_MAX_NR 8

#ifndef __ASSEMBLER__

struct smu_data {
	unsigned long addr;
 };

#endif /* __ASSEMBLER__ */

#endif /* _RISCV_ATCSMU_H */
