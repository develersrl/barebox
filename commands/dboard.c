/*
 * Copyright (c) 2015 Mirko Damiani <mirko@develer.com>, Develer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <common.h>
#include <command.h>
#include <errno.h>
#include <gpio.h>
#include <string.h>
#include <mach/iomux.h>


static int do_dboard_nand_protect(int argc, char **argv)
{
	const char *action = argv[1];
	const char *on = "on";
	const char *off = "off";
	int value, ret;

	if (argc != 2)
		return COMMAND_ERROR_USAGE;

	if (memcmp(action, on, strlen(on)) == 0)
		value = 0;
	else if (memcmp(action, off, strlen(off)) == 0)
		value = 1;
	else
		return COMMAND_ERROR_USAGE;

	ret = gpio_direction_output(AT91_PIN_PD18, value);
	if (ret) {
		printf("ERROR: could not drive NAND write protection gpio: %s\n", errno_str());
		return 1;
	}

	return 0;
}

BAREBOX_CMD_START(dboard_nand_protect)
	.cmd		= do_dboard_nand_protect,
	BAREBOX_CMD_DESC("Enables or disables NAND write protection")
	BAREBOX_CMD_OPTS("[on|off]")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
BAREBOX_CMD_END
