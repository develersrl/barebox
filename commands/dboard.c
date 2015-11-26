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
#include <complete.h>
#include <environment.h>
#include <errno.h>
#include <fs.h>
#include <fcntl.h>
#include <getopt.h>
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


/*
 *   dboard_info command
 */

struct db_model_t {
	uint16_t rev;
	uint16_t ram;
	uint16_t flash;
};

static const struct db_model_t db_models[] = {
	{ 0, 512, 512 },
	{ 1, 512, 512 },
	{ 2, 128, 128 },
	{ 3, 128, 128 },
	{ 4, 256, 512 }
};


static int do_read_eeprom(
                          const char *dev,  // Device to read
                          loff_t start,     // Offset
                          loff_t size,      // Size in bytes
                          void *buff,       // Destination buffer
                          int stride        // Size of a chunk (for endianness)
                          )
{
	int fd;
	int r;
	int ret = 0;

	fd = open_and_lseek(dev, stride | O_RDONLY, start);
	if (fd < 0)
		return 1;

	r = read(fd, buff, size);
	if (r != size)
		ret = 1;

	close(fd);
	return ret;
}

static int do_read_mac(uint8_t mac[])
{
	return do_read_eeprom("/dev/eeprom1", 0x9A, 6, mac, O_RWSIZE_1);
}

static int do_read_uuid(uint8_t uuid[])
{
	return do_read_eeprom("/dev/eeprom1", 0x80, 16, uuid, O_RWSIZE_1);
}

static int do_read_protocol(uint16_t * pcol)
{
	return do_read_eeprom("/dev/eeprom0", 0x00, 2, pcol, O_RWSIZE_2);
}

static int do_read_rev(uint16_t * rev)
{
	return do_read_eeprom("/dev/eeprom0", 0x02, 2, rev, O_RWSIZE_2);
}

static int do_read_date(uint64_t * date)
{
	return do_read_eeprom("/dev/eeprom0", 0x04, 8, date, O_RWSIZE_8);
}

static void do_print_hex(uint8_t buff[], size_t size)
{
	size_t i;

	if (size > 0) {
		printf("%02X", buff[0]);
		for (i = 1; i < size; ++i)
			printf(":%02X", buff[i]);
	}
}

static int do_dbinfo(int argc, char *argv[])
{
	char model[128];
	uint8_t mac[6];
	uint8_t uuid[16];
	uint16_t pcol, rev;
	uint64_t date;

	int opt;
	int revonly = 0;
	char *varname = 0;

	while ((opt = getopt(argc, argv, "rv:")) > 0) {
		switch (opt) {
		case 'r':
			revonly = 1;
			break;
		case 'v':
			varname = optarg;
			break;
		default:
			return 1;
		}
	}


	if (do_read_mac(mac) || do_read_uuid(uuid) || do_read_protocol(&pcol) ||
	    do_read_rev(&rev) || do_read_date(&date))
		return 1;


	/* EEPROM magic pattern or new EEPROM */
	if (rev == 0x0302 || rev == 0xffff)
	{
		fprintf(stderr, "Not tested. Unknown revision.\n");
		return 1;
	}
	else
	{
		// Option "-r"
		if (revonly) {
			printf("r%hu\n", rev);
			return 0;
		}

		// Option "-v"
		if (varname) {
			sprintf(model, "Model: r%hu, RAM %huMB, Flash %huMB",
				rev, db_models[rev].ram, db_models[rev].flash);
			setenv(varname, model);
			return 0;
		}

		  printf("Code:      DBB%huR%huF-R%hu", db_models[rev].ram, db_models[rev].flash, rev);
		printf("\nRevision:  %hu", rev);
		printf("\nRAM:       %huMB", db_models[rev].ram);
		printf("\nFlash:     %huMB", db_models[rev].flash);
		printf("\nProtocol:  %hu", pcol);
		printf("\nTested on: %llu (Unix epoch time)\n", date);

		printf("\nMAC:       "); do_print_hex(mac, 6);
		printf("\nUUID:      "); do_print_hex(uuid, 16);
		printf("\n");
	}

	return 0;
}


BAREBOX_CMD_HELP_START(dboard_info)
BAREBOX_CMD_HELP_TEXT("dboard_info [-r|-v <var>]")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT ("-r",  "print revision only")
BAREBOX_CMD_HELP_OPT ("-v",  "set environment variable with human-readable model string")
BAREBOX_CMD_HELP_END


BAREBOX_CMD_START(dboard_info)
	.cmd		= do_dbinfo,
	BAREBOX_CMD_DESC("Show information about this DevelBoard")
	BAREBOX_CMD_GROUP(CMD_GRP_INFO)
	BAREBOX_CMD_COMPLETE(empty_complete)
	BAREBOX_CMD_HELP(cmd_dboard_info_help)
BAREBOX_CMD_END
