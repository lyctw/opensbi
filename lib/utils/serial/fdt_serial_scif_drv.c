/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2022 Renesas Electronics Corporation
 *
 * Authors:
 */

#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/serial/fdt_serial.h>
#include <sbi_utils/serial/scif_drv.h>

static int serial_scif_drv_init(void *fdt, int nodeoff,
				const struct fdt_match *match)
{
	// int rc;
	// struct platform_uart_data uart = { 0 };

	// rc = fdt_parse_uart_node(fdt, nodeoff, &uart);
	// if (rc)
	// 	return rc;

	// return scif_drv_init(uart.addr, uart.freq, uart.baud);

	return scif_init(RZF_SCIF_DEFAULT_ADDR, RZF_SCIF_DEFAULT_FREQUENCY,
				  RZF_SCIF_DEFAULT_BAUDRATE);
}

static const struct fdt_match serial_scif_drv_match[] = {
	{ .compatible = "renesas,scif-r9a07g043f" },
	{},
};

struct fdt_serial fdt_serial_scif_drv = {
	.match_table = serial_scif_drv_match,
	.init	     = serial_scif_drv_init,
};
