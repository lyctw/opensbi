/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2021 Renesas Electronics Corporation
 */
/*
 * File Name    : scif_drv.h
 * Version      : 1.0
 * Description  : SCIF driver header file.
 */

/*
 Includes   <System Includes> , "Project Includes"
 */

/*
 Macro definitions
 */
#ifndef __SERIAL_SCIF_DRV_H__
#define __SERIAL_SCIF_DRV_H__

/*
 Global Typedef definitions
 */
#define RZF_SCIF_DEFAULT_ADDR 0x1004B800
#define RZF_SCIF_DEFAULT_FREQUENCY 100000000
#define RZF_SCIF_DEFAULT_BAUDRATE 115200
/*
 External global variables
 */

/*
 Exported global functions
 */
int scif_init(unsigned long base, unsigned long clk, unsigned long baudrate);
void scif_put_char(char outChar);

#endif /* __SERIAL_SCIF_DRV_H__ */
