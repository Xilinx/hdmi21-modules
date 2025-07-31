// SPDX-License-Identifier: GPL-2.0
/*
 * TI TMDS1204 retimer driver
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
#include "x_vfmc.h"
#include <linux/of_platform.h>
#include <linux/of_address.h>

#define DRIVER_NAME "ti_tmds1204-rx"

#define to_ti_tmds1204rx(_hw)	container_of(_hw, struct ti_tmds1204rx, hw)
struct ti_tmds1204rx *rxdata;

struct reg_fields {
	u16 dev_type;
	u8 addr;
	u8 val;
};

enum {
	RX_TI_R1_INIT = 0, // program 6 registers
	RX_TI_TMDS_14_L_R1 = RX_TI_R1_INIT + 9,
	RX_TI_TMDS_14_H_R1 = RX_TI_TMDS_14_L_R1 + 14 - 1,
	RX_TI_TMDS_20_R1 = RX_TI_TMDS_14_H_R1 + 14 - 1,
	RX_TI_FRL_3G_R1 = RX_TI_TMDS_20_R1 + 14 - 1,
	RX_TI_FRL_6G_3_R1 = RX_TI_FRL_3G_R1 + 14 - 1,
	RX_TI_FRL_6G_4_R1 = RX_TI_FRL_6G_3_R1 + 14 - 1,
	RX_TI_FRL_8G_R1 = RX_TI_FRL_6G_4_R1 + 14 - 1,
	RX_TI_FRL_10G_R1 = RX_TI_FRL_8G_R1 + 14 - 1,
	RX_TI_FRL_12G_R1 = RX_TI_FRL_10G_R1 + 14 - 1,
};

/*
 * This table contains the values to be programmed to TI_TMDS1204 device.
 * Each entry is of the format:
 * 1) Device Type
 * 2) Register addr
 * 3) val
 */
const struct reg_fields ti_tmds1204rx_regs[] = {
	{RX_TI_R1_INIT, 0x0A, 0x4E},
	{RX_TI_R1_INIT, 0x0B, 0x43},
	{RX_TI_R1_INIT, 0x0C, 0x70},
	{RX_TI_R1_INIT, 0x0D, 0xE3},
	{RX_TI_R1_INIT, 0x0E, 0x97},
	{RX_TI_R1_INIT, 0x1E, 0x00},
	{RX_TI_R1_INIT, 0x11, 0x0F},
	{RX_TI_R1_INIT, 0x09, 0x00},
	{RX_TI_R1_INIT, 0xF8, 0x03},

	{RX_TI_TMDS_14_L_R1, 0x0A, 0x4E},
	{RX_TI_TMDS_14_L_R1, 0x0D, 0xE3},
	{RX_TI_TMDS_14_L_R1, 0x0E, 0x17},
	{RX_TI_TMDS_14_L_R1, 0x12, 0x03},
	{RX_TI_TMDS_14_L_R1, 0x13, 0x00},
	{RX_TI_TMDS_14_L_R1, 0x14, 0x03},
	{RX_TI_TMDS_14_L_R1, 0x15, 0x05},
	{RX_TI_TMDS_14_L_R1, 0x16, 0x03},
	{RX_TI_TMDS_14_L_R1, 0x17, 0x05},
	{RX_TI_TMDS_14_L_R1, 0x18, 0x03},
	{RX_TI_TMDS_14_L_R1, 0x19, 0x05},
	{RX_TI_TMDS_14_L_R1, 0x20, 0x00},
	{RX_TI_TMDS_14_L_R1, 0x31, 0x00},

	{RX_TI_TMDS_14_H_R1, 0x0A, 0x4E},
	{RX_TI_TMDS_14_H_R1, 0x0D, 0xE3},
	{RX_TI_TMDS_14_H_R1, 0x0E, 0x17},
	{RX_TI_TMDS_14_H_R1, 0x12, 0x03},
	{RX_TI_TMDS_14_H_R1, 0x13, 0x00},
	{RX_TI_TMDS_14_H_R1, 0x14, 0x03},
	{RX_TI_TMDS_14_H_R1, 0x15, 0x05},
	{RX_TI_TMDS_14_H_R1, 0x16, 0x03},
	{RX_TI_TMDS_14_H_R1, 0x17, 0x05},
	{RX_TI_TMDS_14_H_R1, 0x18, 0x03},
	{RX_TI_TMDS_14_H_R1, 0x19, 0x05},
	{RX_TI_TMDS_14_H_R1, 0x20, 0x00},
	{RX_TI_TMDS_14_H_R1, 0x31, 0x00},

	{RX_TI_TMDS_20_R1, 0x0A, 0x4E},
	{RX_TI_TMDS_20_R1, 0x0D, 0xE3},
	{RX_TI_TMDS_20_R1, 0x0E, 0x17},
	{RX_TI_TMDS_20_R1, 0x12, 0x03},
	{RX_TI_TMDS_20_R1, 0x13, 0x00},
	{RX_TI_TMDS_20_R1, 0x14, 0x03},
	{RX_TI_TMDS_20_R1, 0x15, 0x05},
	{RX_TI_TMDS_20_R1, 0x16, 0x03},
	{RX_TI_TMDS_20_R1, 0x17, 0x05},
	{RX_TI_TMDS_20_R1, 0x18, 0x03},
	{RX_TI_TMDS_20_R1, 0x19, 0x05},
	{RX_TI_TMDS_20_R1, 0x20, 0x02},
	{RX_TI_TMDS_20_R1, 0x31, 0x00},

	{RX_TI_FRL_3G_R1, 0x0A, 0x0E},
	{RX_TI_FRL_3G_R1, 0x0D, 0xE3},
	{RX_TI_FRL_3G_R1, 0x0E, 0x17},
	{RX_TI_FRL_3G_R1, 0x12, 0x03},
	{RX_TI_FRL_3G_R1, 0x13, 0x00},
	{RX_TI_FRL_3G_R1, 0x14, 0x03},
	{RX_TI_FRL_3G_R1, 0x15, 0x05},
	{RX_TI_FRL_3G_R1, 0x16, 0x03},
	{RX_TI_FRL_3G_R1, 0x17, 0x05},
	{RX_TI_FRL_3G_R1, 0x18, 0x03},
	{RX_TI_FRL_3G_R1, 0x19, 0x05},
	{RX_TI_FRL_3G_R1, 0x20, 0x00},
	{RX_TI_FRL_3G_R1, 0x31, 0x01},

	{RX_TI_FRL_6G_3_R1, 0x0A, 0x0E},
	{RX_TI_FRL_6G_3_R1, 0x0D, 0xE3},
	{RX_TI_FRL_6G_3_R1, 0x0E, 0x17},
	{RX_TI_FRL_6G_3_R1, 0x12, 0x03},
	{RX_TI_FRL_6G_3_R1, 0x13, 0x00},
	{RX_TI_FRL_6G_3_R1, 0x14, 0x03},
	{RX_TI_FRL_6G_3_R1, 0x15, 0x05},
	{RX_TI_FRL_6G_3_R1, 0x16, 0x03},
	{RX_TI_FRL_6G_3_R1, 0x17, 0x05},
	{RX_TI_FRL_6G_3_R1, 0x18, 0x03},
	{RX_TI_FRL_6G_3_R1, 0x19, 0x05},
	{RX_TI_FRL_6G_3_R1, 0x20, 0x00},
	{RX_TI_FRL_6G_3_R1, 0x31, 0x02},

	{RX_TI_FRL_6G_4_R1, 0x0A, 0x0E},
	{RX_TI_FRL_6G_4_R1, 0x0D, 0xE3},
	{RX_TI_FRL_6G_4_R1, 0x0E, 0x17},
	{RX_TI_FRL_6G_4_R1, 0x12, 0x03},
	{RX_TI_FRL_6G_4_R1, 0x13, 0x05},
	{RX_TI_FRL_6G_4_R1, 0x14, 0x03},
	{RX_TI_FRL_6G_4_R1, 0x15, 0x05},
	{RX_TI_FRL_6G_4_R1, 0x16, 0x03},
	{RX_TI_FRL_6G_4_R1, 0x17, 0x05},
	{RX_TI_FRL_6G_4_R1, 0x18, 0x03},
	{RX_TI_FRL_6G_4_R1, 0x19, 0x05},
	{RX_TI_FRL_6G_4_R1, 0x20, 0x00},
	{RX_TI_FRL_6G_4_R1, 0x31, 0x03},

	{RX_TI_FRL_8G_R1, 0x0A, 0x0E},
	{RX_TI_FRL_8G_R1, 0x0D, 0xF3},
	{RX_TI_FRL_8G_R1, 0x0E, 0x17},
	{RX_TI_FRL_8G_R1, 0x12, 0x01},
	{RX_TI_FRL_8G_R1, 0x13, 0x00},
	{RX_TI_FRL_8G_R1, 0x14, 0x03},
	{RX_TI_FRL_8G_R1, 0x15, 0x05},
	{RX_TI_FRL_8G_R1, 0x16, 0x01},
	{RX_TI_FRL_8G_R1, 0x17, 0x00},
	{RX_TI_FRL_8G_R1, 0x18, 0x01},
	{RX_TI_FRL_8G_R1, 0x19, 0x00},
	{RX_TI_FRL_8G_R1, 0x20, 0x00},
	{RX_TI_FRL_8G_R1, 0x31, 0x04},

	{RX_TI_FRL_10G_R1, 0x0A, 0x0E},
	{RX_TI_FRL_10G_R1, 0x0D, 0xF3},
	{RX_TI_FRL_10G_R1, 0x12, 0x02},
	{RX_TI_FRL_10G_R1, 0x13, 0x00},
	{RX_TI_FRL_10G_R1, 0x14, 0x01},
	{RX_TI_FRL_10G_R1, 0x15, 0x00},
	{RX_TI_FRL_10G_R1, 0x16, 0x00},
	{RX_TI_FRL_10G_R1, 0x17, 0x01},
	{RX_TI_FRL_10G_R1, 0x18, 0x02},
	{RX_TI_FRL_10G_R1, 0x19, 0x00},
	{RX_TI_FRL_10G_R1, 0x20, 0x00},
	{RX_TI_FRL_10G_R1, 0x31, 0x05},

	{RX_TI_FRL_12G_R1, 0x0A, 0x0E},
	{RX_TI_FRL_12G_R1, 0x0D, 0xF3},
	{RX_TI_FRL_12G_R1, 0x12, 0x01},
	{RX_TI_FRL_12G_R1, 0x13, 0x01},
	{RX_TI_FRL_12G_R1, 0x14, 0x01},
	{RX_TI_FRL_12G_R1, 0x15, 0x01},
	{RX_TI_FRL_12G_R1, 0x16, 0x01},
	{RX_TI_FRL_12G_R1, 0x17, 0x01},
	{RX_TI_FRL_12G_R1, 0x18, 0x01},
	{RX_TI_FRL_12G_R1, 0x19, 0x01},
	{RX_TI_FRL_12G_R1, 0x20, 0x00},
	{RX_TI_FRL_12G_R1, 0x31, 0x06},
};

static const struct regmap_config ti_tmds1204rx_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

/*
 * struct ti_tmds1204 - ti_tmds1204 device structure
 * @client: Pointer to I2C client
 * @regmap: Pointer to regmap structure
 * @lock: Mutex structure
 * @mode_index: Resolution mode index
 */
struct ti_tmds1204rx {
	struct clk_hw hw;
	struct i2c_client *client;
	struct regmap *regmap;
	struct mutex lock; /* mutex lock for operations */
	u32 mode_index;
};

static inline int ti_tmds1204rx_read_reg(struct ti_tmds1204rx *priv, u8 addr, u8 *val)
{
	int err = 0;

	err = regmap_read(priv->regmap, addr, (unsigned int *)val);
	if (err)
		dev_dbg(&priv->client->dev,
			"i2c read failed, addr = %x\n", addr);
	return err;
}

static inline int ti_tmds1204rx_write_reg(struct ti_tmds1204rx *priv, u8 addr, u8 val)
{
	int err = 0;

	err = regmap_write(priv->regmap, addr, val);
	if (err) {
		dev_dbg(&priv->client->dev,
			"i2c write failed, addr = %x\n", addr);
	}

	return err;
}

/*
 * ti_tmds1204rx_linerate_conf - configure line rate for TI TMDS1204 RX device
 * @is_frl: Flag indicating if FRL (Fixed Rate Link) mode is enabled
 * @linerate: Line rate in bits per second
 * @is_tx: Direction flag (0 for RX, 1 for TX)
 * @lanes: Number of lanes for the configuration
 *
 * This function configures the TI TMDS1204 RX device based on the specified
 * line rate and protocol type. It selects appropriate register settings
 * based on FRL mode, line rate, and board type.
 *
 * Return: 0 on success, negative error code on failure
 */
int ti_tmds1204rx_linerate_conf(u8 is_frl, u64 linerate, u8 is_tx, u8 lanes)
{
	u32 linerate_mbps;
	u32 i = 0;
	u16 dev_type = 0xffff;
	int ret = 0;
	u8 revision = 1;
	u8 board_type;

	/* Convert line rate from bps to Mbps for easier comparison */
	linerate_mbps = (u32)((u64)linerate / 1000000);

	dev_info(&rxdata->client->dev, "linerate %llu lineratembps %u lanes %d\n",
		 linerate, linerate_mbps, lanes);

	/* Select device configuration based on revision and protocol type */
	switch (revision) {
	case 1:
		/* Check if FRL (Fixed Rate Link) mode is enabled */
		if (is_frl == 1) {
			/* Select FRL configuration based on line rate */
			if (linerate_mbps == 12000) {
				dev_type = RX_TI_FRL_12G_R1;
			} else if (linerate_mbps == 10000) {
				dev_type = RX_TI_FRL_10G_R1;
			} else if (linerate_mbps == 8000) {
				dev_type = RX_TI_FRL_8G_R1;
			} else if (linerate_mbps == 6000) {
				/* 6G FRL has different configs for 3 vs 4 lanes */
				if (lanes == 4)
					dev_type = RX_TI_FRL_6G_4_R1;
				else
					dev_type = RX_TI_FRL_6G_3_R1;
			} else if (linerate_mbps == 3000) {
				dev_type = RX_TI_FRL_3G_R1;
			}
		} else {
			/* Use TMDS configuration for non-FRL mode */
			dev_type = RX_TI_TMDS_20_R1;
		}
		break;
	default:
		break;
	}

	board_type = xfmcdev_board_type();

	/* Find the starting index in register table for the selected device type */
	while (dev_type != ti_tmds1204rx_regs[i].dev_type) {
		i++;
		if (i > sizeof(ti_tmds1204rx_regs))
			break;
	}
	/* Program all registers for the selected device configuration */
	while (dev_type == ti_tmds1204rx_regs[i].dev_type) {
		ret = ti_tmds1204rx_write_reg(rxdata, ti_tmds1204rx_regs[i].addr,
					      ti_tmds1204rx_regs[i].val);
		if (ret)
			return ret;
		/* Apply board-specific register overrides */
		if (board_type == BASE_BOARD_VERSAL_EDGE) {
			/* VERSAL_EDGE: VEK385 and VEK280 */
			ret = ti_tmds1204rx_write_reg(rxdata, 0x0E, 0x17);
			ret = ti_tmds1204rx_write_reg(rxdata, 0x09, 0x02);
		} else if (dev_type == RX_TI_FRL_12G_R1) {
			/* Special handling for 12G FRL mode */
			ret = ti_tmds1204rx_write_reg(rxdata, 0x18, 0x02);
			if (board_type == BASE_BOARD_ZCU102) {
				/* ZCU102 specific configuration */
				ret = ti_tmds1204rx_write_reg(rxdata, 0x13, 0x00);
			} else if (board_type == BASE_BOARD_ZCU106) {
				/* ZCU106 specific configuration */
				ret = ti_tmds1204rx_write_reg(rxdata, 0x13, 0x05);
				ret = ti_tmds1204rx_write_reg(rxdata, 0x16, 0x00);
				ret = ti_tmds1204rx_write_reg(rxdata, 0x17, 0x03);
			}
		}

		if (ret)
			return ret;
		i++;
	}

	dev_info(&rxdata->client->dev, "linerate conf done\n");
	return ret;
}
EXPORT_SYMBOL_GPL(ti_tmds1204rx_linerate_conf);

/*
 * ti_tmds1204rx_init - initialize TI TMDS1204 RX device with default settings
 * @priv: Pointer to ti_tmds1204rx device structure
 * @revision: Device revision number (1 for R1)
 * @is_tx: Direction flag (0 for RX, 1 for TX)
 *
 * This function initializes the TI TMDS1204 RX device with basic configuration
 * settings based on the device revision. It programs the initialization
 * register set and applies board-specific overrides.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ti_tmds1204rx_init(struct ti_tmds1204rx *priv, u8 revision, u8 is_tx)
{
	int ret = 0;
	u16 dev_type = 0xffff;
	u32 i = 0;
	u8 board_type;

	/* Select initialization register set based on device revision */
	switch (revision) {
	case 1:
		dev_type = RX_TI_R1_INIT;
		break;
	default:
		break;
	}
	/* Find the starting index in register table for initialization sequence */
	while (dev_type != ti_tmds1204rx_regs[i].dev_type) {
		i++;
		if (i > sizeof(ti_tmds1204rx_regs))
			break;
	}

	board_type = xfmcdev_board_type();

	/* Program all initialization registers for the device */
	while (dev_type == ti_tmds1204rx_regs[i].dev_type) {
		ret = ti_tmds1204rx_write_reg(priv, ti_tmds1204rx_regs[i].addr,
					      ti_tmds1204rx_regs[i].val);

		/* Apply board-specific register overrides for Versal Edge boards */
		if (board_type == BASE_BOARD_VERSAL_EDGE) {
			/* Configure specific registers for VEK385 and VEK280 boards */
			ret = ti_tmds1204rx_write_reg(rxdata, 0x0E, 0x17);
			ret = ti_tmds1204rx_write_reg(rxdata, 0x09, 0x02);
		}

		if (ret)
			return ret;
		i++;
	}

	dev_dbg(&priv->client->dev, "i2c write OK!!");
	return ret;
}

/*
 * ti_tmds1204rx_probe - probe function for TI TMDS1204 RX I2C device
 * @client: Pointer to I2C client device structure
 *
 * This function is called when the I2C device is detected and matched with
 * the driver. It allocates memory for device data, initializes regmap for
 * I2C communication, and performs initial device configuration.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ti_tmds1204rx_probe(struct i2c_client *client)
{
	int ret;

	rxdata = devm_kzalloc(&client->dev, sizeof(*rxdata), GFP_KERNEL);
	if (!rxdata)
		return -ENOMEM;

	/* Initialize device structure with client and mutex */
	rxdata->client = client;
	mutex_init(&rxdata->lock);

	/* Initialize regmap for I2C register access */
	rxdata->regmap = devm_regmap_init_i2c(client, &ti_tmds1204rx_regmap_config);
	if (IS_ERR(rxdata->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(rxdata->regmap));
		ret = -ENODEV;
		goto err_regmap;
	}

	i2c_set_clientdata(client, rxdata);

	dev_dbg(&client->dev, "init ti_tmds1204-rx\n");
	ret = ti_tmds1204rx_init(rxdata, 1, 0);
	if (ret) {
		dev_err(&client->dev, "failed to init ti_tmds1204-rx\n");
		return ret;
	}

	dev_dbg(&client->dev, "driver probed!!\n");
	return 0;

err_regmap:
	/* Cleanup mutex on regmap initialization failure */
	mutex_destroy(&rxdata->lock);
	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
static int ti_tmds1204rx_remove(struct i2c_client *client)
{
	return 0;
}
#else
static void ti_tmds1204rx_remove(struct i2c_client *client)
{
	return;
}
#endif

static const struct of_device_id ti_tmds1204rx_of_id_table[] = {
	{ .compatible = "ti_tmds1204,ti_tmds1204-rx" },
	{ }
};
MODULE_DEVICE_TABLE(of, ti_tmds1204rx_of_id_table);

static const struct i2c_device_id ti_tmds1204rx_id[] = {
	{ "ti_tmds1204rx", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ti_tmds1204rx_id);

static struct i2c_driver ti_tmds1204rx_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table	= ti_tmds1204rx_of_id_table,
	},
	.probe		= ti_tmds1204rx_probe,
	.remove		= ti_tmds1204rx_remove,
	.id_table	= ti_tmds1204rx_id,
};

void ti_tmds1204rx_exit(void)
{
	i2c_del_driver(&ti_tmds1204rx_i2c_driver);
}
EXPORT_SYMBOL_GPL(ti_tmds1204rx_exit);

/*
 * ti_tmds1204rx_entry - Entry point function for TI TMDS1204 RX driver
 *
 * This function initializes and registers the TI TMDS1204 RX I2C driver.
 * It checks for device tree compatibility and registers the driver with
 * the I2C subsystem.
 *
 * Return: 0 on success, negative error code on failure
 */
int ti_tmds1204rx_entry(void)
{
	struct device_node *node;

	node = of_find_matching_node(NULL, ti_tmds1204rx_of_id_table);
	if (!node) {
		pr_err("TI_TMDS1204_RX: Compatible DT node 'ti_tmds1204,ti_tmds1204-rx' not found.\n");
		return -ENODEV;
	}
	of_node_put(node);
	return i2c_add_driver(&ti_tmds1204rx_i2c_driver);
}
EXPORT_SYMBOL_GPL(ti_tmds1204rx_entry);

MODULE_DESCRIPTION("TI TMDS1204 retimer chip driver");
MODULE_LICENSE("GPL v2");
