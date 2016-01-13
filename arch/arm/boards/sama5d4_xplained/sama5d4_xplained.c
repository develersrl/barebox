/*
 * SAMA5D4 XPLAINED ULTRA board configuration.
 *
 * Copyright (C) 2014 Atmel Corporation,
 *		      Bo Shen <voice.shen@atmel.com>
 *
 * Licensed under GPLv2 or later.
 */

#include <command.h>
#include <common.h>
#include <net.h>
#include <init.h>
#include <environment.h>
#include <asm/armlinux.h>
#include <asm/sections.h>
#include <partition.h>
#include <fs.h>
#include <fcntl.h>
#include <io.h>
#include <mach/hardware.h>
#include <nand.h>
#include <linux/sizes.h>
#include <linux/mtd/nand.h>
#include <mach/board.h>
#include <mach/at91sam9_smc.h>
#include <gpio.h>
#include <mach/iomux.h>
#include <mach/at91_pmc.h>
#include <mach/at91_rstc.h>
#include <mach/at91sam9x5_matrix.h>
#include <input/qt1070.h>
#include <readkey.h>
#include <spi/spi.h>
#include <i2c/i2c.h>
#include <i2c/at24.h>
#include <linux/clk.h>
#include <linux/stat.h>
#include <envfs.h>

#if defined(CONFIG_NAND_ATMEL)
static struct atmel_nand_data nand_pdata = {
	.ale		= 21,
	.cle		= 22,
	.det_pin	= -EINVAL,
	.rdy_pin	= -EINVAL,
	.enable_pin	= -EINVAL,
	.wp_pin		= AT91_PIN_PD18,
	.ecc_mode	= NAND_ECC_HW,
	.has_pmecc	= 1,
	.on_flash_bbt	= 1,
};

/**
 * ONFi 1.0 Timing Modes
 *
 *            MODE  0    MODE  1      MODE  2      MODE  3      MODE  4      MODE  5
 *            Min Max    Min Max      Min Max      Min Max      Min Max      Min Max     Unit
 *    tADL    200        100          100          100          70           70           ns
 *    tALH    20         10           10           5            5            5            ns
 *    tALS    50         25           15           10           10           10           ns
 *    tAR     25         10           10           10           10           10           ns
 *    tCEA        100        45           30           25           25           25       ns
 *    tCH     20         10           10           5            5            5            ns
 *    tCHZ        100        50           50           50           30           30       ns
 *    tCLH    20         10           10           5            5            5            ns
 *    tCLR    20         10           10           10           10           10           ns
 *    tCLS    50         25           15           10           10           10           ns
 *    tCOH    0          15           15           15           15           15           ns
 *    tCS     70         35           25           25           20           15           ns
 *    tDH     20         10           5            5            5            5            ns
 *    tDS     40         20           15           10           10           7            ns
 *    tFEAT       1          1            1            1            1            1        us
 *    tIR     10         0            0            0            0            0            ns
 *    tRC     100        50           35           30           25           20           ns
 *    tREA        40         30           25           20           20           16       ns
 *    tREH    30         15           15           10           10           7            ns
 *    tRHOH   0          15           15           15           15           15           ns
 *    tRHW    200        100          100          100          100          100          ns
 *    tRHZ        200        100          100          100          100          100      ns
 *    tRLOH   0          0            0            0            5            5            ns
 *    tRP     50         25           17           15           12           10           ns
 *    tRR     40         20           20           20           20           20           ns
 *    tRST        1000       5/10/500     5/10/500     5/10/500     5/10/500     5/10/500 us
 *    tWB         200        100          100          100          100          100      ns
 *    tWC     100        45           35           30           25           20           ns
 *    tWH     30         15           15           10           10           7            ns
 *    tWHR    120        80           80           60           60           60           ns
 *    tWP     50         25           17           15           12           10           ns
 *    tWW     100        100          100          100          100          100          ns
 *
 * Timing Mode 0 shall be supported by all ONFi nand devices.
 *
 * The first Read Parameter Page command shall use tR = 200us and tCCS = 500ns,
 * afterwards correct values are extracted from ONFi page.
 */

/**
 * SMC clock frequency is 90MHz (cycle is ~11ns).
 *
 * -----------            ------- NCS / NRD
 * |         |\_________/       |
 * |         |         |        |
 * |  setup  |  pulse  |  hold  |
 * |         |  (tWP)  |  (tWH) |
 * |         |  (tRP)  | (tREH) |
 * |                            |
 * |            cycle           |
 * |            (tWC)           |
 * |            (tRC)           |
 */

static struct sam9_smc_config cm_nand_smc_config = {
	.ncs_read_setup  = 1,
	.ncs_write_setup = 1,
	.nrd_setup       = 1,
	.nwe_setup       = 1,

	.ncs_read_pulse  = 4,
	.ncs_write_pulse = 4,
	.nrd_pulse       = 3,
	.nwe_pulse       = 3,

	.read_cycle      = 4,
	.write_cycle     = 4,

	.mode            = AT91_SMC_READMODE | AT91_SMC_WRITEMODE | AT91_SMC_EXNWMODE_DISABLE,
	.tdf_cycles      = 3,

	.tclr            = 2,
	.tadl            = 7,
	.tar             = 2,
	.ocms            = 0,
	.trr             = 3,
	.twb             = 7,
	.rbnsel          = 3,
	.nfsel           = 1,
};

static void ek_add_device_nand(void)
{
	struct clk *clk = clk_get(NULL, "smc_clk");

	clk_enable(clk);

	/* configure chip-select 3 (NAND) */
	sama5_smc_configure(0, 3, &cm_nand_smc_config);

	at91_add_device_nand(&nand_pdata);
}
#else
static void ek_add_device_nand(void) {}
#endif

#if defined(CONFIG_DRIVER_NET_MACB) || defined(CONFIG_DRIVER_NET_MACB1)
#if defined(CONFIG_DRIVER_NET_MACB)
static struct macb_platform_data macb0_pdata = {
	.phy_interface = PHY_INTERFACE_MODE_RMII,
	.phy_addr = 0,
};
#endif

#if defined(CONFIG_DRIVER_NET_MACB1)
static struct macb_platform_data macb1_pdata = {
	.phy_interface = PHY_INTERFACE_MODE_RMII,
	.phy_addr = CONFIG_DRIVER_PHYADDR_MACB1,
};
#endif

static void ek_init_ethaddr_i2c(int ethid, int addr, int reg)
{
	struct i2c_adapter *adapter;
	struct i2c_client client;
	char mac[6];
	char str[sizeof("xx:xx:xx:xx:xx:xx")];
	int ret;

	adapter = i2c_get_adapter(0);
	if (!adapter) {
		pr_warning("warning: No MAC address set: i2c adapter not found\n");
		return;
	}

	client.adapter = adapter;
	client.addr = addr;

	ret = i2c_read_reg(&client, reg, mac, sizeof(mac));
	if (ret != sizeof(mac)) {
		pr_warning("warning: No MAC address set: EEPROM read error\n");
		return;
	}

	eth_register_ethaddr(ethid, mac);

	ethaddr_to_string(mac, str);
	pr_info("MAC address read from EEPROM: %s\n", str);
}

static void ek_add_device_eth(void)
{
#if defined(CONFIG_DRIVER_NET_MACB)
	ek_init_ethaddr_i2c(0, 0x58, 0x9A);
	at91_add_device_eth(0, &macb0_pdata);
#endif

#if defined(CONFIG_DRIVER_NET_MACB1)
	at91_add_device_eth(1, &macb1_pdata);
#endif
}
#else
static void ek_add_device_eth(void) {}
#endif

#if defined(CONFIG_DRIVER_VIDEO_ATMEL_HLCD)
static struct fb_videomode at91_tft_vga_modes[] = {
	{
		.name		= "TM4301",
		.refresh	= 60,
		.xres		= 480,		.yres		= 272,
		.pixclock	= KHZ2PICOS(9000),

		.left_margin	= 2,		.right_margin	= 2,
		.upper_margin	= 2,		.lower_margin	= 2,
		.hsync_len	= 41,		.vsync_len	= 11,

		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	{
		.name		= "RFF700H-AIW",
		.refresh	= 60,
		.xres		= 800,		.yres		= 480,
		.pixclock	= KHZ2PICOS(32260),

		.left_margin	= 2,		.right_margin	= 2,
		.upper_margin	= 2,		.lower_margin	= 2,
		.hsync_len	= 256,		.vsync_len	= 45,

		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
};

/* Output mode is TFT 24 bits */
#define BPP_OUT_DEFAULT_LCDCFG5	(LCDC_LCDCFG5_MODE_OUTPUT_24BPP)

#define MAX_FB_SIZE SZ_2M
#define FB_ADDRESS  0x23000000

static struct atmel_lcdfb_platform_data ek_lcdc_data = {
	.lcdcon_is_backlight		= true,
	.default_bpp			= 16,
	.default_dmacon			= ATMEL_LCDC_DMAEN,
	.default_lcdcon2		= BPP_OUT_DEFAULT_LCDCFG5,
	.guard_time			= 9,
	.lcd_wiring_mode		= ATMEL_LCDC_WIRING_RGB,
	.mode_list			= at91_tft_vga_modes,
	.num_modes			= ARRAY_SIZE(at91_tft_vga_modes),
	.panel_id			= -1,
	.fixed_fb                       = (void *)(FB_ADDRESS),
	.fixed_fb_size                  = MAX_FB_SIZE
};

static void ek_add_device_lcdc(void)
{
	/* On sama5d4 xplained ultra board, we use 24bits connection */
	at91_set_A_periph(AT91_PIN_PA0, 0);	/* LCDD0 */
	at91_set_A_periph(AT91_PIN_PA1, 0);	/* LCDD1 */
	at91_set_A_periph(AT91_PIN_PA8, 0);	/* LCDD8 */
	at91_set_A_periph(AT91_PIN_PA9, 0);	/* LCDD9 */
	at91_set_A_periph(AT91_PIN_PA16, 0);	/* LCDD16 */
	at91_set_A_periph(AT91_PIN_PA17, 0);	/* LCDD17 */

	at91_add_device_lcdc(&ek_lcdc_data);
}
#else
static void ek_add_device_lcdc(void) {}
#endif

#if defined(CONFIG_MCI_ATMEL)
static struct atmel_mci_platform_data mci1_data = {
	.bus_width	= 4,
	.detect_pin	= -EINVAL,
	.wp_pin		= -EINVAL,
};

static void ek_add_device_mci(void)
{
	/* MMC1 */
	at91_add_device_mci(1, &mci1_data);

	/* power on MCI1 */
	at91_set_gpio_output(AT91_PIN_PE4, 0);
}
#else
static void ek_add_device_mci(void) {}
#endif

#if defined(CONFIG_I2C_GPIO)
#if defined(CONFIG_KEYBOARD_QT1070)
struct qt1070_platform_data qt1070_pdata = {
	.irq_pin	= AT91_PIN_PE10,
};
#endif

#if defined(CONFIG_EEPROM_AT24)
/**
 * AT24MAC402 Standard 2-Kbit EEPROM
 * First half address range: 0x00-0x7F
 * Second half address range: 0x80-0xFF
 */
static struct at24_platform_data at24mac402_eeprom = {
	.byte_len = SZ_2K / 8,
	.page_size = 16,
	.flags = AT24_FLAG_READONLY,
};

/**
 * AT24MAC402 Extended Memory
 * 128-bit Serial Number: 0x80-0x8F (16 bytes)
 * EUI-48 Value: 0x9A-0x9F (6 bytes)
 * EUI-64 Value: 0x98-0x9F (8 bytes)
 */
static struct at24_platform_data at24mac402_eui_sn = {
	.byte_len = 256,
	.page_size = 1,
	.flags = AT24_FLAG_READONLY,
};
#endif

static struct i2c_board_info i2c_devices[] = {
#if defined(CONFIG_KEYBOARD_QT1070)
	{
		.platform_data = &qt1070_pdata,
		I2C_BOARD_INFO("qt1070", 0x1b),
	},
#endif
#if defined(CONFIG_EEPROM_AT24)
	{
		.platform_data = &at24mac402_eeprom,
		I2C_BOARD_INFO("at24", 0x50),
	},
	{
		.platform_data = &at24mac402_eui_sn,
		I2C_BOARD_INFO("at24", 0x58),
	},
#endif
};

static void ek_add_device_i2c(void)
{
#if defined(CONFIG_KEYBOARD_QT1070)
	at91_set_gpio_input(qt1070_pdata.irq_pin, 0);
	at91_set_deglitch(qt1070_pdata.irq_pin, 1);
#endif

	at91_add_device_i2c(0, i2c_devices, ARRAY_SIZE(i2c_devices));
}
#else
static void ek_add_device_i2c(void) {}
#endif

#if defined(CONFIG_DRIVER_SPI_ATMEL)
static const struct spi_board_info ek_spi_devices[] = {
	{
		.name		= "m25p80",
		.chip_select	= 0,
		.max_speed_hz	= 30 * 1000 * 1000,
		.bus_num	= 0,
	}
};

static unsigned spi0_standard_cs[] = { AT91_PIN_PC3 };
static struct at91_spi_platform_data spi_pdata = {
	.chipselect = spi0_standard_cs,
	.num_chipselect = ARRAY_SIZE(spi0_standard_cs),
};

static void ek_add_device_spi(void)
{
	spi_register_board_info(ek_spi_devices, ARRAY_SIZE(ek_spi_devices));
	at91_add_device_spi(0, &spi_pdata);
}
#else
static void ek_add_device_spi(void) {}
#endif

#ifdef CONFIG_LED_GPIO
struct gpio_led leds[] = {
	{
		.gpio	= AT91_PIN_PB31,
		.active_low	= 0,
		.led	= {
			.name = "d10",
		},
	},
};

static void ek_add_led(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		at91_set_gpio_output(leds[i].gpio, leds[i].active_low);
		led_gpio_register(&leds[i]);
	}
	led_set_trigger(LED_TRIGGER_HEARTBEAT, &leds[0].led);
}
#else
static void ek_add_led(void) {}
#endif

static int sama5d4_xplained_mem_init(void)
{
	at91_add_device_sdram(0);

	return 0;
}
mem_initcall(sama5d4_xplained_mem_init);

static const struct devfs_partition sama5d4_xplained_nand0_partitions[] = {
	{
		.offset = 0x00000,
		.size = SZ_256K,
		.flags = DEVFS_PARTITION_FIXED,
		.name = "at91bootstrap_raw",
		.bbname = "at91bootstrap",
	}, {
		.offset = DEVFS_PARTITION_APPEND,
		.size = SZ_512K,
		.flags = DEVFS_PARTITION_FIXED,
		.name = "bootloader_raw",
		.bbname = "bootloader",
	}, {
		.offset = DEVFS_PARTITION_APPEND,
		.size = SZ_256K,
		.flags = DEVFS_PARTITION_FIXED,
		.name = "env_raw",
		.bbname = "env0",
	}, {
		.offset = DEVFS_PARTITION_APPEND,
		.size = SZ_256K,
		.flags = DEVFS_PARTITION_FIXED,
		.name = "env_raw1",
		.bbname = "env1",
	}, {
		/* sentinel */
	}
};

static int sama5d4_xplained_devices_init(void)
{
	ek_add_device_i2c();
	ek_add_device_nand();
	ek_add_led();
	ek_add_device_eth();
	ek_add_device_spi();
	ek_add_device_mci();

	devfs_create_partitions("nand0", sama5d4_xplained_nand0_partitions);

	return 0;
}
device_initcall(sama5d4_xplained_devices_init);

static int sama5d4_xplained_console_init(void)
{
	barebox_set_model("Atmel sama5d4_xplained");
	barebox_set_hostname("sama5d4_xplained");

	at91_register_uart(4, 0);

	return 0;
}
console_initcall(sama5d4_xplained_console_init);

static int sama5d4_xplained_main_clock(void)
{
	at91_set_main_clock(12000000);

	return 0;
}
pure_initcall(sama5d4_xplained_main_clock);

static int sama5d4_xplained_env_init(void)
{
	struct stat s;
	const char *diskdev = "/dev/disk0.0";
	char *bootenv = "/boot/barebox.env";
	int ret;

	device_detect_by_name("mci1");

	ret = stat(diskdev, &s);
	if (ret) {
		pr_info("no %s, using default env\n", diskdev);
		return 0;
	}

	mkdir("/boot", 0666);
	ret = mount(diskdev, "fat", "/boot", NULL);
	if (ret) {
		pr_warning("failed to mount %s\n", diskdev);
		return 0;
	}

	ret = stat(bootenv, &s);
	if (ret) {
		pr_info("no %s, using default env\n", bootenv);
		return 0;
	}

	pr_info("found boot env %s\n", bootenv);
	default_environment_path_set(bootenv);

	return 0;
}
late_initcall(sama5d4_xplained_env_init);

static int sama5d4_xplained_postenv_init(void)
{
	struct stat s;
	char *env_cfg = "/env/config";
	char *src_cmd[] = { "source", env_cfg };
	const char *var;
	long panel_id;
	int ret;

	ret = stat(env_cfg, &s);
	if (ret) {
		pr_info("no configuration file found\n");
		return 0;
	}

	ret = execute_command(2, src_cmd);
	if (ret) {
		pr_warning("couldn't source config file\n");
		return 0;
	}

	var = getenv("panel_id");
	if (!var) {
		pr_info("no panel id defined\n");
		return 0;
	}

	panel_id = simple_strtol(var, NULL, 0);
	if (panel_id < 0 || panel_id >= ARRAY_SIZE(at91_tft_vga_modes)) {
		pr_warning("invalid panel id %ld\n", panel_id);
		return 0;
	}

	pr_info("found panel with id %ld, initializing lcd\n", panel_id);

	memset((void*)(FB_ADDRESS), 0, MAX_FB_SIZE);

	ek_lcdc_data.panel_id = panel_id;
	ek_add_device_lcdc();

	return 0;
}
postenvironment_initcall(sama5d4_xplained_postenv_init);
