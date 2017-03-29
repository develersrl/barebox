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
#include <linux/err.h>
#include <net.h>
#include <string.h>


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


/******************************************************************************
 * dboard_net_discover
 ******************************************************************************/

#define PORT_DISCOVER         13992

#define DISCOVER_SERVICE_NFS  1
#define DISCOVER_SERVICE_TFTP 2

#define DP_OP_REQUEST 1
#define DP_OP_REPLY 2

#define DISCOVER_STATE_SEARCHING 1
#define DISCOVER_STATE_FOUND 2

static uint64_t discover_start;
static int discover_service;
static struct net_connection *discover_con;
static char discover_serial[32];
static int discover_state;
static IPaddr_t discover_serverip;

struct discoverpkt {
	uint8_t dp_op;
	uint8_t dp_service;
	uint16_t dp_spare1;     /* alignment */
	char dp_serial[32];
};

static int discover_request(int service)
{
	unsigned char *payload = net_udp_get_payload(discover_con);
	struct discoverpkt *dp;

	debug("netdiscover broadcast\n");

	dp = (struct discoverpkt *)payload;
	dp->dp_op = DP_OP_REQUEST;
	dp->dp_service = (uint8_t)service;
	memcpy(dp->dp_serial, discover_serial, 32);

	return net_udp_send(discover_con, sizeof(*dp));
}

static void discover_handler(void *ctx, char *packet, unsigned int len) {
	struct discoverpkt *dp = (struct discoverpkt *)net_eth_to_udp_payload(packet);
	struct iphdr *iph = net_eth_to_iphdr(packet);

	len = net_eth_to_udplen(packet);

	if (discover_state != DISCOVER_STATE_SEARCHING) {
		return;
	}

	if (dp->dp_op != DP_OP_REPLY || dp->dp_service != discover_service) {
		return;
	}
	if (strncmp(dp->dp_serial, discover_serial, 32)) {
		debug("DiscoverHandler: invalid serial, ignoring\n");
		return;
	}

	discover_serverip = net_read_ip((void*)&iph->saddr);
	discover_state = DISCOVER_STATE_FOUND;
}


static int do_dbnetdiscover(int argc, char *argv[])
{
	int ret = 0;
	int retries = 8;
	uint8_t mac[6];

	if (argc < 2) {
		fprintf(stderr, "error: service not specified\n");
		return 1;
	}

	if (!strcmp(argv[1], "nfs")) {
		discover_service = DISCOVER_SERVICE_NFS;
	} else if (!strcmp(argv[1], "tftp")) {
		discover_service = DISCOVER_SERVICE_TFTP;
	} else {
		fprintf(stderr, "error: service unknown\n");
		return 1;
	}

	ret = do_read_mac(mac);
	if (ret) {
		fprintf(stderr, "cannot read eeprom: %d\n", ret);
		return 1;
	}

	memset(discover_serial, 0, sizeof(discover_serial));
	sprintf(discover_serial, "%02X%02X%02X", mac[3],mac[4],mac[5]);

	discover_con = net_udp_new(0xffffffff, PORT_DISCOVER, discover_handler, NULL);
	if (IS_ERR(discover_con)) {
		ret = PTR_ERR(discover_con);
		goto out;
	}

	ret = net_udp_bind(discover_con, PORT_DISCOVER);
	if (ret)
		goto out1;

	discover_start = get_time_ns();

	ret = discover_request(discover_service);
	if (ret)
		goto out1;

	discover_state = DISCOVER_STATE_SEARCHING;
	while (discover_state != DISCOVER_STATE_FOUND) {
		if (ctrlc()) {
			ret = -EINTR;
			goto out1;
		}

		if (!retries) {
			ret = -ETIMEDOUT;
			goto out1;
		}

		net_poll();
		if (is_timeout(discover_start, 1 * SECOND)) {
			discover_start = get_time_ns();
			printf("T ");
			ret = discover_request(discover_service);
			retries--;
			if (ret)
				goto out1;
		}
	}

	printf("Found server IP: %s\n", ip_to_string(discover_serverip));
	net_set_serverip(discover_serverip);

out1:
	net_unregister(discover_con);

out:
	if (ret)
		debug("discover failed: %s\n", strerror(-ret));

	return ret;
}


BAREBOX_CMD_HELP_START(dboard_net_discover)
BAREBOX_CMD_HELP_TEXT("dboard_net_discover <service>")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Service can be either \"nfs\" or \"tftp\"")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(dboard_net_discover)
	.cmd		= do_dbnetdiscover,
	BAREBOX_CMD_DESC("Discover network services exposed by the dboard tool")
	BAREBOX_CMD_OPTS("SERVICE")
	BAREBOX_CMD_GROUP(CMD_GRP_NET)
	BAREBOX_CMD_COMPLETE(command_var_complete)
BAREBOX_CMD_END
