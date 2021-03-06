/*
 * Copyright (C) 2017 Develer Srl <pietro@develer.com>
 *
 * based on arch/arm/boards/freescale-mx6-sabrelite/board.c
 * Copyright (C) 2012 Steffen Trumtrar, Pengutronix
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <init.h>
#include <environment.h>
#include <mach/imx6-regs.h>
#include <gpio.h>
#include <asm/armlinux.h>
#include <generated/mach-types.h>
#include <partition.h>
#include <linux/phy.h>
#include <asm/io.h>
#include <asm/mmu.h>
#include <mach/generic.h>
#include <linux/sizes.h>
#include <net.h>
#include <mach/imx6.h>
#include <mach/devices-imx6.h>
#include <mach/iomux-mx6.h>
#include <spi/spi.h>
#include <mach/spi.h>
#include <mach/usb.h>
#include <bbu.h>
#include <mach/bbu.h>
#include <dboard.h>

int dboard_get_serial(uint8_t *out, size_t cnt)
{
	uint64_t uid;

	/* TODO: find a better, non-breaking way to handle failure here. */
	uid = imx_uid();
	if (uid == -ENODEV)
		return -ENODEV;

	memcpy(out, &uid, min(cnt, sizeof(uid)));

	return 0;
}

static int uq7_mem_init(void)
{
	if (!of_machine_is_compatible("seco,imx6dl-uq7-a75-j"))
		return 0;

	arm_add_mem_device("ram0", 0x10000000, SZ_1G);

	return 0;
}
mem_initcall(uq7_mem_init);

static int uq7_devices_init(void)
{
	int ret;
	char *env_path;

	if (!of_machine_is_compatible("seco,imx6dl-uq7-a75-j"))
		return 0;

	env_path = asprintf("/chosen/environment-mmc%d",
				bootsource_get_instance());

	ret = of_device_enable_path(env_path);
	if (ret < 0)
		pr_warn("Failed to enable environment partition '%s' (%d)\n",
			env_path, ret);

	free(env_path);

	return 0;
}
device_initcall(uq7_devices_init);

static int uq7_coredevices_init(void)
{
	if (!of_machine_is_compatible("seco,imx6dl-uq7-a75-j"))
		return 0;

	barebox_set_model("SECO i.MX6 uQ7-A75-J");
	barebox_set_hostname("seco");

	return 0;
}
coredevice_initcall(uq7_coredevices_init);
