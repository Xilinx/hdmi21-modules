// SPDX-License-Identifier: GPL-2.0
/*
 * ONSEMI NB7NQ621M cable redriver driver
 *
 * Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.
 *
 */
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include "x_vfmc.h"

#define DRIVER_NAME "onsemi-rx"

#define to_onsemirx(_hw)	container_of(_hw, struct onsemirx, hw)
struct onsemirx *os_rxdata;

enum {
	RX_R0 = 69,
	RX_R1_TMDS_14 = 153,
	RX_R1_TMDS_20 = 162,
	RX_R1_FRL = 171,
	RX_R2_TMDS_14 = 232,
	RX_R2_TMDS_20 = 241,
	RX_R2_FRL = 250,
	/* Above these were all early versions of
	 * OnSemi re-driver
	 * All the 21 write registers are added for flexibility
	 */
	RX_R3_TMDS_14 = 355,
	RX_R3_TMDS_20 = RX_R3_TMDS_14 + 21,
	RX_R3_FRL = RX_R3_TMDS_20 + 21,
};

struct onsemi_register_field {
	u16 dev_type;
	u8 addr;
	u8 val;
};

/*
 * This table contains the values to be programmed to ONSEMI device.
 * Each entry is of the format:
 * 1) Device Type
 * 2) Register Address
 * 3) Values
 */
struct onsemi_register_field onsemirx_regs[] = {
	{RX_R0, 0x04, 0xB0},
	{RX_R0, 0x05, 0x0D},
	{RX_R0, 0x06, 0x00},
	{RX_R0, 0x07, 0x32},
	{RX_R0, 0x08, 0x0B},
	{RX_R0, 0x09, 0x32},
	{RX_R0, 0x0A, 0x0B},
	{RX_R0, 0x0B, 0x0F},
	{RX_R0, 0x0C, 0xAA},
	{RX_R0, 0x0D, 0x00},
	{RX_R0, 0x0E, 0x03},
	{RX_R0, 0x0F, 0x00},
	{RX_R0, 0x10, 0x00},
	{RX_R0, 0x11, 0x03},
	{RX_R0, 0x12, 0x00},
	{RX_R0, 0x13, 0x00},
	{RX_R0, 0x14, 0x03},
	{RX_R0, 0x15, 0x00},
	{RX_R0, 0x16, 0x00},
	{RX_R0, 0x17, 0x03},
	{RX_R0, 0x18, 0x00},

	{RX_R1_TMDS_14, 0x0A, 0x20},
	{RX_R1_TMDS_14, 0x0B, 0x0F},
	{RX_R1_TMDS_14, 0x0C, 0x00},
	{RX_R1_TMDS_14, 0x0D, 0x00},
	{RX_R1_TMDS_14, 0x0E, 0x03},
	{RX_R1_TMDS_14, 0x0F, 0x21},
	{RX_R1_TMDS_14, 0x10, 0x2A},
	{RX_R1_TMDS_14, 0x11, 0x0F},
	{RX_R1_TMDS_14, 0x12, 0xAA},

	{RX_R1_TMDS_20, 0x0A, 0x20},
	{RX_R1_TMDS_20, 0x0B, 0x0F},
	{RX_R1_TMDS_20, 0x0C, 0x00},
	{RX_R1_TMDS_20, 0x0D, 0x00},
	{RX_R1_TMDS_20, 0x0E, 0x03},
	{RX_R1_TMDS_20, 0x0F, 0x00},
	{RX_R1_TMDS_20, 0x10, 0x00},
	{RX_R1_TMDS_20, 0x11, 0x0F},
	{RX_R1_TMDS_20, 0x12, 0xAA},

	{RX_R1_FRL, 0x0A, 0x20},
	{RX_R1_FRL, 0x0B, 0x0F},
	{RX_R1_FRL, 0x0C, 0x00},
	{RX_R1_FRL, 0x0D, 0x00},
	{RX_R1_FRL, 0x0E, 0x07},
	{RX_R1_FRL, 0x0F, 0x20},
	{RX_R1_FRL, 0x10, 0x01},
	{RX_R1_FRL, 0x11, 0x0F},
	{RX_R1_FRL, 0x12, 0xAA},

	{RX_R2_TMDS_14, 0x0A, 0x20},
	{RX_R2_TMDS_14, 0x0B, 0x0F},
	{RX_R2_TMDS_14, 0x0C, 0x00},
	{RX_R2_TMDS_14, 0x0D, 0x00},
	{RX_R2_TMDS_14, 0x0E, 0x03},
	{RX_R2_TMDS_14, 0x0F, 0x21},
	{RX_R2_TMDS_14, 0x10, 0x2A},
	{RX_R2_TMDS_14, 0x11, 0x0F},
	{RX_R2_TMDS_14, 0x12, 0xAA},

	{RX_R2_TMDS_20, 0x0A, 0x20},
	{RX_R2_TMDS_20, 0x0B, 0x0F},
	{RX_R2_TMDS_20, 0x0C, 0x00},
	{RX_R2_TMDS_20, 0x0D, 0x00},
	{RX_R2_TMDS_20, 0x0E, 0x03},
	{RX_R2_TMDS_20, 0x0F, 0x00},
	{RX_R2_TMDS_20, 0x10, 0x00},
	{RX_R2_TMDS_20, 0x11, 0x0F},
	{RX_R2_TMDS_20, 0x12, 0xAA},

	{RX_R2_FRL, 0x0A, 0xA0},
	{RX_R2_FRL, 0x0B, 0x0F},
	{RX_R2_FRL, 0x0C, 0x00},
	{RX_R2_FRL, 0x0D, 0x20},
	{RX_R2_FRL, 0x0E, 0x07},
	{RX_R2_FRL, 0x0F, 0x20},
	{RX_R2_FRL, 0x10, 0x00},
	{RX_R2_FRL, 0x11, 0x0F},
	{RX_R2_FRL, 0x12, 0xAA},
	{RX_R2_FRL, 0x13, 0x20},
	{RX_R2_FRL, 0x14, 0x00},
	{RX_R2_FRL, 0x15, 0x00},
	{RX_R2_FRL, 0x16, 0x21},
	{RX_R2_FRL, 0x17, 0x00},
	{RX_R2_FRL, 0x18, 0x00},
	{RX_R2_FRL, 0x19, 0x20},
	{RX_R2_FRL, 0x1A, 0x00},
	{RX_R2_FRL, 0x1B, 0x00},
	{RX_R2_FRL, 0x1C, 0x20},
	{RX_R2_FRL, 0x1D, 0x07},

	{RX_R3_TMDS_14, 0x0A, 0x34},
	{RX_R3_TMDS_14, 0x0B, 0x0D},
	{RX_R3_TMDS_14, 0x0C, 0x00},
	{RX_R3_TMDS_14, 0x0D, 0x00},
	{RX_R3_TMDS_14, 0x0E, 0x03},
	{RX_R3_TMDS_14, 0x0F, 0x21},
	{RX_R3_TMDS_14, 0x10, 0x2A},
	{RX_R3_TMDS_14, 0x11, 0x0F},
	{RX_R3_TMDS_14, 0x12, 0x00},
	{RX_R3_TMDS_14, 0x13, 0x00},
	{RX_R3_TMDS_14, 0x14, 0x03},
	{RX_R3_TMDS_14, 0x15, 0x00},
	{RX_R3_TMDS_14, 0x16, 0x00},
	{RX_R3_TMDS_14, 0x17, 0x03},
	{RX_R3_TMDS_14, 0x18, 0x00},
	{RX_R3_TMDS_14, 0x19, 0x00},
	{RX_R3_TMDS_14, 0x1A, 0x03},
	{RX_R3_TMDS_14, 0x1B, 0x00},
	{RX_R3_TMDS_14, 0x1C, 0x00},
	{RX_R3_TMDS_14, 0x1D, 0x03},
	{RX_R3_TMDS_14, 0x1E, 0x00},

	{RX_R3_TMDS_20, 0x0A, 0x34},
	{RX_R3_TMDS_20, 0x0B, 0x0D},
	{RX_R3_TMDS_20, 0x0C, 0x00},
	{RX_R3_TMDS_20, 0x0D, 0x00},
	{RX_R3_TMDS_20, 0x0E, 0x03},
	{RX_R3_TMDS_20, 0x0F, 0x00},
	{RX_R3_TMDS_20, 0x10, 0x00},
	{RX_R3_TMDS_20, 0x11, 0x0F},
	{RX_R3_TMDS_20, 0x12, 0x00},
	{RX_R3_TMDS_20, 0x13, 0x00},
	{RX_R3_TMDS_20, 0x14, 0x03},
	{RX_R3_TMDS_20, 0x15, 0x00},
	{RX_R3_TMDS_20, 0x16, 0x00},
	{RX_R3_TMDS_20, 0x17, 0x03},
	{RX_R3_TMDS_20, 0x18, 0x00},
	{RX_R3_TMDS_20, 0x19, 0x00},
	{RX_R3_TMDS_20, 0x1A, 0x03},
	{RX_R3_TMDS_20, 0x1B, 0x00},
	{RX_R3_TMDS_20, 0x1C, 0x00},
	{RX_R3_TMDS_20, 0x1D, 0x03},
	{RX_R3_TMDS_20, 0x1E, 0x00},

	{RX_R3_FRL, 0x0A, 0x24},
	{RX_R3_FRL, 0x0B, 0x0D},
	{RX_R3_FRL, 0x0C, 0x00},
	{RX_R3_FRL, 0x0D, 0x20},
	{RX_R3_FRL, 0x0E, 0x07},
	{RX_R3_FRL, 0x0F, 0x20},
	{RX_R3_FRL, 0x10, 0x00},
	{RX_R3_FRL, 0x11, 0x0F},
	{RX_R3_FRL, 0x12, 0xAA},
	{RX_R3_FRL, 0x13, 0x21},
	{RX_R3_FRL, 0x14, 0x00},
	{RX_R3_FRL, 0x15, 0x00},
	{RX_R3_FRL, 0x16, 0x21},
	{RX_R3_FRL, 0x17, 0x00},
	{RX_R3_FRL, 0x18, 0x00},
	{RX_R3_FRL, 0x19, 0x21},
	{RX_R3_FRL, 0x1A, 0x00},
	{RX_R3_FRL, 0x1B, 0x00},
	{RX_R3_FRL, 0x1C, 0x20},
	{RX_R3_FRL, 0x1D, 0x07},
	{RX_R3_FRL, 0x1E, 0x00},

};

static const struct regmap_config onsemirx_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

/*
 * struct onsemirx - onsemi device structure
 * @client: Pointer to I2C client
 * @ctrls: onsemi control structure
 * @regmap: Pointer to regmap structure
 * @lock: Mutex structure
 * @mode_index: Resolution mode index
 * @board_type: type of board
 */
struct onsemirx {
	struct clk_hw hw;
	struct i2c_client *client;
	struct regmap *regmap;
	struct mutex lock; /* mutex lock for operations */
	u32 mode_index;
	u8 board_type;
};

static inline int onsemirx_read_reg(struct onsemirx *priv, u8 addr, u8 *val)
{
	int err = 0;

	err = regmap_read(priv->regmap, addr, (unsigned int *)val);
	if (err)
		dev_dbg(&priv->client->dev,
			"i2c read failed, addr = %x\n", addr);
	return err;
}

static inline int onsemirx_write_reg(struct onsemirx *priv, u8 addr, u8 val)
{
	int err = 0;

	err = regmap_write(priv->regmap, addr, val);
	if (err) {
		dev_dbg(&priv->client->dev,
			"i2c write failed, addr = %x\n", addr);
	}

	return err;
}

/**
 * onsemirx_linerate_conf - Configure ONSEMI RX redriver for specific line rate
 * @is_frl: Flag indicating FRL mode (1) vs TMDS mode (0)
 * @line_rate: Line rate in bps (bits per second)
 * @is_tx: Flag indicating TX mode (currently unused for RX configuration)
 *
 * This function configures the ONSEMI NB7NQ621M cable redriver for a specific
 * line rate and signaling mode. It selects the appropriate register configuration
 * based on:
 * - Hardware revision (R1, R2, R3)
 * - Signaling mode (FRL vs TMDS)
 * - Line rate thresholds (14 Gbps vs 20 Gbps for TMDS)
 * - Board-specific optimizations (ZCU106, VCK190)
 *
 * The function programs the device with pre-defined register values optimized
 * for signal integrity at the specified line rate.
 *
 * Return: 0 on success, positive error code on failure
 */
int onsemirx_linerate_conf(u8 is_frl, u64 line_rate, u8 is_tx)
{
	u32 linerate_mbps;
	u32 i = 0;
	u16 dev_type = 0xffff;
	int ret = 1;
	u8 revision = 3;	/* OnSemi TX-Mezz R3 hardware revision */

	/* Convert line rate from bps to Mbps for threshold comparison */
	linerate_mbps = (u32)((u64)line_rate / 100000);	/* Remove one zero */
	pr_debug("linerate %llu lineratembps %u\n", line_rate, linerate_mbps);

	/* Select device configuration type based on hardware revision and mode */
	switch (revision) {
	case 0:
		/* Legacy R0 revision - configuration disabled */
		/* dev_type = RX_R0; */
		break;
	case 1:
		/* R1 revision: Select FRL or TMDS mode with rate threshold */
		if (is_frl == 1) {
			dev_type = RX_R1_FRL;
		} else {
			/* TMDS mode: 20 Gbps threshold determines configuration */
			if (linerate_mbps > 3400)
				dev_type = RX_R1_TMDS_20;
			else
				dev_type = RX_R1_TMDS_14;
		}
		break;
	case 2:
		/* R2 revision: Select FRL or TMDS mode with rate threshold */
		if (is_frl == 1) {
			dev_type = RX_R2_FRL;
		} else {
			/* TMDS mode: 20 Gbps threshold determines configuration */
			if (linerate_mbps > 3400)
				dev_type = RX_R2_TMDS_20;
			else
				dev_type = RX_R2_TMDS_14;
		}
		break;
	case 3:
		/* R3 revision: Select FRL or TMDS mode with rate threshold */
		if (is_frl == 1) {
			dev_type = RX_R3_FRL;
		} else {
			/* TMDS mode: 20 Gbps threshold determines configuration */
			if (linerate_mbps > 3400)
				dev_type = RX_R3_TMDS_20;
			else
				dev_type = RX_R3_TMDS_14;
		}
		break;
	default:
		break;
	}

	/* Find the starting index for the selected device type in register table */
	while (dev_type != onsemirx_regs[i].dev_type) {
		i++;
		if (i > sizeof(onsemirx_regs))
			break;
	}

	/* Program all registers for the selected device configuration */
	while (dev_type == onsemirx_regs[i].dev_type) {
		/* Apply board-specific register value overrides */
		if (os_rxdata->board_type == BASE_BOARD_ZCU106) {
			/* ZCU106 board optimizations */
			if (dev_type == RX_R2_FRL) {
				/* R2 FRL mode optimizations for ZCU106 */
				if (onsemirx_regs[i].addr == 0x1C)
					onsemirx_regs[i].val = 0x03;
				else if (onsemirx_regs[i].addr == 0x1D)
					onsemirx_regs[i].val = 0x00;
			} else if (dev_type == RX_R3_FRL) {
				/* R3 FRL mode optimizations for ZCU106 */
				if (onsemirx_regs[i].addr == 0x0D)
					onsemirx_regs[i].val = 0x31;
				else if (onsemirx_regs[i].addr == 0x0E)
					onsemirx_regs[i].val = 0x0A;
				else if (onsemirx_regs[i].addr == 0x0F)
					onsemirx_regs[i].val = 0x31;
				else if (onsemirx_regs[i].addr == 0x10)
					onsemirx_regs[i].val = 0x05;
			}
		} else if (os_rxdata->board_type == BASE_BOARD_VCK190) {
			/* VCK190 board optimizations */
			if (dev_type == RX_R3_FRL) {
				/* R3 FRL mode optimizations for VCK190 */
				if (onsemirx_regs[i].addr == 0x0D)
					onsemirx_regs[i].val = 0x10;
				else if (onsemirx_regs[i].addr == 0x0E)
					onsemirx_regs[i].val = 0x0A;
				else if (onsemirx_regs[i].addr == 0x0F)
					onsemirx_regs[i].val = 0x31;
				else if (onsemirx_regs[i].addr == 0x10)
					onsemirx_regs[i].val = 0x05;
			}
		}

		/* Write register value to ONSEMI device */
		ret = onsemirx_write_reg(os_rxdata, onsemirx_regs[i].addr,
					 onsemirx_regs[i].val);
		if (ret)
			return ret;
		i++;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(onsemirx_linerate_conf);

static int onsemirx_init(struct onsemirx *priv, u8 revision, u8 is_tx)
{
	int ret = 1;
	u16 dev_type = 0xffff;
	u32 i = 0;

	switch (revision) {
	case 0:
		dev_type = RX_R0;
		break;
	case 1:
		dev_type = RX_R1_TMDS_14;
		break;
	case 2:
		dev_type = RX_R2_TMDS_14;
		break;
	case 3:
		dev_type = RX_R3_TMDS_14;
		break;
	default:
		break;
	}

	while (dev_type != onsemirx_regs[i].dev_type) {
		i++;
		if (i > sizeof(onsemirx_regs))
			break;
	}

	while (dev_type == onsemirx_regs[i].dev_type) {
		ret = onsemirx_write_reg(priv, onsemirx_regs[i].addr,
					 onsemirx_regs[i].val);

		if (ret)
			return ret;
		i++;
	}
	return ret;
}

/*
 * onsemirx_probe - Probe function for ONSEMI NB7NQ621M cable redriver
 * @client: Pointer to I2C client device
 *
 * This function initializes the ONSEMI NB7NQ621M cable redriver device.
 * It performs the following operations:
 * - Allocates memory for the device structure
 * - Initializes mutex for thread-safe operations
 * - Sets up regmap for I2C communication
 * - Configures the device with default register values for revision 3
 * - Detects and stores the board type for board-specific optimizations
 * - Sets up clock framework integration if specified in device tree
 *
 * The probe function is called when the I2C subsystem detects a matching
 * device based on the compatible string "onsemi,onsemi-rx" in the device tree.
 *
 * Return: 0 on success, negative error code on failure
 */
static int onsemirx_probe(struct i2c_client *client)
{
	struct clk_init_data init;
	int ret, err;
	u32 initial_fout;

	/* initialize onsemi */
	os_rxdata = devm_kzalloc(&client->dev, sizeof(*os_rxdata), GFP_KERNEL);
	if (!os_rxdata)
		return -ENOMEM;

	mutex_init(&os_rxdata->lock);

	if (of_property_read_string(client->dev.of_node, "clock-output-names",
				    &init.name))
		init.name = client->dev.of_node->name;

	/* initialize regmap */
	os_rxdata->regmap = devm_regmap_init_i2c(client, &onsemirx_regmap_config);
	if (IS_ERR(os_rxdata->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(os_rxdata->regmap));
		ret = -ENODEV;
		goto err_regmap;
	}

	i2c_set_clientdata(client, os_rxdata);

	os_rxdata->board_type = xfmcdev_board_type();

	dev_dbg(&client->dev, "init onsemi-rx with default values\n");
	/* revision Pass4 Silicon, VFMC Active HDMI TX Mezz (R2) */
	ret = onsemirx_init(os_rxdata, 3, false);
	if (ret) {
		dev_err(&client->dev, "failed to init onsemi-rx\n");
		return ret;
	}

	/* Read the requested initial output frequency from device tree */
	if (!of_property_read_u32(client->dev.of_node, "clock-frequency",
				  &initial_fout)) {
		err = clk_set_rate(os_rxdata->hw.clk, initial_fout);
		if (err) {
			of_clk_del_provider(client->dev.of_node);
			return err;
		}
	}
	return 0;

err_regmap:
	mutex_destroy(&os_rxdata->lock);
	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
static int onsemirx_remove(struct i2c_client *client)
{
	return 0;
}
#else
static void onsemirx_remove(struct i2c_client *client)
{
	return;
}
#endif

static const struct of_device_id onsemirx_of_id_table[] = {
	{ .compatible = "onsemi,onsemi-rx" },
	{ }
};
MODULE_DEVICE_TABLE(of, onsemirx_of_id_table);

static const struct i2c_device_id onsemirx_id[] = {
	{ "onsemirx", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, onsemirx_id);

static struct i2c_driver onsemirx_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table	= onsemirx_of_id_table,
	},
	.probe		= onsemirx_probe,
	.remove		= onsemirx_remove,
	.id_table	= onsemirx_id,
};

void onsemirx_exit(void)
{
	i2c_del_driver(&onsemirx_i2c_driver);
}
EXPORT_SYMBOL_GPL(onsemirx_exit);

int onsemirx_entry(void)
{
	struct device_node *node;

	node = of_find_matching_node(NULL, onsemirx_of_id_table);
	if (!node) {
		pr_err("OnsemiRX: Compatible DT node 'onsemi,onsemi-rx' not found.\n");
		return -ENODEV;
	}
	of_node_put(node);
	return i2c_add_driver(&onsemirx_i2c_driver);
}
EXPORT_SYMBOL_GPL(onsemirx_entry);

MODULE_DESCRIPTION("ONSEMI NB7NQ621M cable redriver driver");
MODULE_LICENSE("GPL v2");
