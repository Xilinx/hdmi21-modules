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
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include "x_vfmc.h"

#define DRIVER_NAME "ti_tmds1204-tx"

#define DELAY_MS	5

#define to_ti_tmds1204tx(_hw)	container_of(_hw, struct ti_tmds1204tx, hw)
struct ti_tmds1204tx *txdata;

struct reg_fields {
	u16 dev_type;
	u8 addr;
	u8 val;
};

enum {
	TX_TI_R1_INIT = 0, // program 6 registers
	TX_TI_TMDS_14_L_R1 = TX_TI_R1_INIT + 7, // 13 registers are programmed
	TX_TI_TMDS_14_H_R1 = TX_TI_TMDS_14_L_R1 + 13,
	TX_TI_TMDS_20_R1 = TX_TI_TMDS_14_H_R1 + 13,
	TX_TI_FRL_3G_R1 = TX_TI_TMDS_20_R1 + 13,
	TX_TI_FRL_6G_3_R1 = TX_TI_FRL_3G_R1 + 13,
	TX_TI_FRL_6G_4_R1 = TX_TI_FRL_6G_3_R1 + 13,
	TX_TI_FRL_8G_R1 = TX_TI_FRL_6G_4_R1 + 13,
	TX_TI_FRL_10G_R1 = TX_TI_FRL_8G_R1 + 13,
	TX_TI_FRL_12G_R1 = TX_TI_FRL_10G_R1 + 13,
};

/*
 * This table contains the values to be programmed to TI_TMDS1204 device.
 * Each entry is of the format:
 * 1) Device Type
 * 2) Register addr
 * 3) val
 */
const struct reg_fields ti_tmds1204tx_regs[] = {
	{TX_TI_R1_INIT, 0x0A, 0x8E},
	{TX_TI_R1_INIT, 0x0B, 0x43},
	{TX_TI_R1_INIT, 0x0C, 0x70},
	{TX_TI_R1_INIT, 0x0D, 0x22},
	{TX_TI_R1_INIT, 0x0E, 0x97},
	{TX_TI_R1_INIT, 0x11, 0x00},
	{TX_TI_R1_INIT, 0x09, 0x00},

	{TX_TI_TMDS_14_L_R1, 0x11, 0x00},
	{TX_TI_TMDS_14_L_R1, 0x0D, 0x22},
	{TX_TI_TMDS_14_L_R1, 0x12, 0x03},
	{TX_TI_TMDS_14_L_R1, 0x13, 0x00},
	{TX_TI_TMDS_14_L_R1, 0x14, 0x03},
	{TX_TI_TMDS_14_L_R1, 0x15, 0x05},
	{TX_TI_TMDS_14_L_R1, 0x16, 0x03},
	{TX_TI_TMDS_14_L_R1, 0x17, 0x05},
	{TX_TI_TMDS_14_L_R1, 0x18, 0x03},
	{TX_TI_TMDS_14_L_R1, 0x19, 0x05},
	{TX_TI_TMDS_14_L_R1, 0x20, 0x00},
	{TX_TI_TMDS_14_L_R1, 0x31, 0x00},
	{TX_TI_TMDS_14_L_R1, 0x11, 0x0F},

	{TX_TI_TMDS_14_H_R1, 0x11, 0x00},
	{TX_TI_TMDS_14_H_R1, 0x0D, 0x22},
	{TX_TI_TMDS_14_H_R1, 0x12, 0x03},
	{TX_TI_TMDS_14_H_R1, 0x13, 0x00},
	{TX_TI_TMDS_14_H_R1, 0x14, 0x03},
	{TX_TI_TMDS_14_H_R1, 0x15, 0x05},
	{TX_TI_TMDS_14_H_R1, 0x16, 0x03},
	{TX_TI_TMDS_14_H_R1, 0x17, 0x05},
	{TX_TI_TMDS_14_H_R1, 0x18, 0x03},
	{TX_TI_TMDS_14_H_R1, 0x19, 0x05},
	{TX_TI_TMDS_14_H_R1, 0x20, 0x00},
	{TX_TI_TMDS_14_H_R1, 0x31, 0x00},
	{TX_TI_TMDS_14_H_R1, 0x11, 0x0F},

	{TX_TI_TMDS_20_R1, 0x11, 0x00},
	{TX_TI_TMDS_20_R1, 0x0D, 0x22},
	{TX_TI_TMDS_20_R1, 0x12, 0x03},
	{TX_TI_TMDS_20_R1, 0x13, 0x00},
	{TX_TI_TMDS_20_R1, 0x14, 0x03},
	{TX_TI_TMDS_20_R1, 0x15, 0x05},
	{TX_TI_TMDS_20_R1, 0x16, 0x03},
	{TX_TI_TMDS_20_R1, 0x17, 0x05},
	{TX_TI_TMDS_20_R1, 0x18, 0x03},
	{TX_TI_TMDS_20_R1, 0x19, 0x05},
	{TX_TI_TMDS_20_R1, 0x20, 0x02},
	{TX_TI_TMDS_20_R1, 0x31, 0x00},
	{TX_TI_TMDS_20_R1, 0x11, 0x0F},

	{TX_TI_FRL_3G_R1, 0x11, 0x00},
	{TX_TI_FRL_3G_R1, 0x0D, 0x22},
	{TX_TI_FRL_3G_R1, 0x12, 0x03},
	{TX_TI_FRL_3G_R1, 0x13, 0x00},
	{TX_TI_FRL_3G_R1, 0x14, 0x03},
	{TX_TI_FRL_3G_R1, 0x15, 0x05},
	{TX_TI_FRL_3G_R1, 0x16, 0x03},
	{TX_TI_FRL_3G_R1, 0x17, 0x05},
	{TX_TI_FRL_3G_R1, 0x18, 0x03},
	{TX_TI_FRL_3G_R1, 0x19, 0x05},
	{TX_TI_FRL_3G_R1, 0x20, 0x00},
	{TX_TI_FRL_3G_R1, 0x31, 0x01},
	{TX_TI_FRL_3G_R1, 0x11, 0x0F},

	{TX_TI_FRL_6G_3_R1, 0x11, 0x00},
	{TX_TI_FRL_6G_3_R1, 0x0D, 0x22},
	{TX_TI_FRL_6G_3_R1, 0x12, 0x03},
	{TX_TI_FRL_6G_3_R1, 0x13, 0x00},
	{TX_TI_FRL_6G_3_R1, 0x14, 0x03},
	{TX_TI_FRL_6G_3_R1, 0x15, 0x05},
	{TX_TI_FRL_6G_3_R1, 0x16, 0x03},
	{TX_TI_FRL_6G_3_R1, 0x17, 0x05},
	{TX_TI_FRL_6G_3_R1, 0x18, 0x03},
	{TX_TI_FRL_6G_3_R1, 0x19, 0x05},
	{TX_TI_FRL_6G_3_R1, 0x20, 0x00},
	{TX_TI_FRL_6G_3_R1, 0x31, 0x02},
	{TX_TI_FRL_6G_3_R1, 0x11, 0x0F},

	{TX_TI_FRL_6G_4_R1, 0x11, 0x00},
	{TX_TI_FRL_6G_4_R1, 0x0D, 0x22},
	{TX_TI_FRL_6G_4_R1, 0x12, 0x03},
	{TX_TI_FRL_6G_4_R1, 0x13, 0x05},
	{TX_TI_FRL_6G_4_R1, 0x14, 0x03},
	{TX_TI_FRL_6G_4_R1, 0x15, 0x05},
	{TX_TI_FRL_6G_4_R1, 0x16, 0x03},
	{TX_TI_FRL_6G_4_R1, 0x17, 0x05},
	{TX_TI_FRL_6G_4_R1, 0x18, 0x03},
	{TX_TI_FRL_6G_4_R1, 0x19, 0x05},
	{TX_TI_FRL_6G_4_R1, 0x20, 0x00},
	{TX_TI_FRL_6G_4_R1, 0x31, 0x03},
	{TX_TI_FRL_6G_4_R1, 0x11, 0x0F},

	{TX_TI_FRL_8G_R1, 0x11, 0x00},
	{TX_TI_FRL_8G_R1, 0x0D, 0x22},
	{TX_TI_FRL_8G_R1, 0x12, 0x03},
	{TX_TI_FRL_8G_R1, 0x13, 0x05},
	{TX_TI_FRL_8G_R1, 0x14, 0x03},
	{TX_TI_FRL_8G_R1, 0x15, 0x05},
	{TX_TI_FRL_8G_R1, 0x16, 0x03},
	{TX_TI_FRL_8G_R1, 0x17, 0x05},
	{TX_TI_FRL_8G_R1, 0x18, 0x03},
	{TX_TI_FRL_8G_R1, 0x19, 0x05},
	{TX_TI_FRL_8G_R1, 0x20, 0x00},
	{TX_TI_FRL_8G_R1, 0x31, 0x04},
	{TX_TI_FRL_8G_R1, 0x11, 0x0F},

	{TX_TI_FRL_10G_R1, 0x11, 0x00},
	{TX_TI_FRL_10G_R1, 0x0D, 0x22},
	{TX_TI_FRL_10G_R1, 0x12, 0x03},
	{TX_TI_FRL_10G_R1, 0x13, 0x05},
	{TX_TI_FRL_10G_R1, 0x14, 0x03},
	{TX_TI_FRL_10G_R1, 0x15, 0x05},
	{TX_TI_FRL_10G_R1, 0x16, 0x03},
	{TX_TI_FRL_10G_R1, 0x17, 0x05},
	{TX_TI_FRL_10G_R1, 0x18, 0x03},
	{TX_TI_FRL_10G_R1, 0x19, 0x05},
	{TX_TI_FRL_10G_R1, 0x20, 0x00},
	{TX_TI_FRL_10G_R1, 0x31, 0x05},
	{TX_TI_FRL_10G_R1, 0x11, 0x0F},

	{TX_TI_FRL_12G_R1, 0x11, 0x00},
	{TX_TI_FRL_12G_R1, 0x0D, 0x22},
	{TX_TI_FRL_12G_R1, 0x12, 0x03},
	{TX_TI_FRL_12G_R1, 0x13, 0x05},
	{TX_TI_FRL_12G_R1, 0x14, 0x03},
	{TX_TI_FRL_12G_R1, 0x15, 0x05},
	{TX_TI_FRL_12G_R1, 0x16, 0x03},
	{TX_TI_FRL_12G_R1, 0x17, 0x05},
	{TX_TI_FRL_12G_R1, 0x18, 0x03},
	{TX_TI_FRL_12G_R1, 0x19, 0x05},
	{TX_TI_FRL_12G_R1, 0x20, 0x00},
	{TX_TI_FRL_12G_R1, 0x31, 0x06},
	{TX_TI_FRL_12G_R1, 0x11, 0x0F},
};

static const struct regmap_config ti_tmds1204tx_regmap_config = {
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
 * @board_type: type of board
 */

struct ti_tmds1204tx {
	struct clk_hw hw;
	struct i2c_client *client;
	struct regmap *regmap;
	struct mutex lock; /* mutex lock for operations */
	u32 mode_index;
	u8 board_type;
};

static inline void msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base * 1000, delay_base * 1000 + 500);
}

static inline int ti_tmds1204tx_read_reg(struct ti_tmds1204tx *priv, u8 addr, u8 *val)
{
	int err = 0;

	err = regmap_read(priv->regmap, addr, (unsigned int *)val);
	if (err) {
		dev_dbg(&priv->client->dev,
			"i2c read failed, addr = %x\n", addr);
	} else {
		dev_dbg(&priv->client->dev,
			"i2c read OK, addr = 0x%02x, val = 0x%02x\n", addr, *val);
	}

	return err;
}

static inline int ti_tmds1204tx_write_reg(struct ti_tmds1204tx *priv, u8 addr, u8 val)
{
	int err = 0;

	msleep(DELAY_MS);
	err = regmap_write(priv->regmap, addr, val);
	if (err) {
		dev_dbg(&priv->client->dev,
			"i2c write failed, addr = %x\n", addr);
	} else {
		dev_dbg(&priv->client->dev,
			"i2c write OK, addr = 0x%02x, val = 0x%02x\n", addr, val);
	}

	return err;
}

/*
 * ti_tmds1204tx_linerate_conf - configure line rate for TI TMDS1204 TX device
 * @is_frl: Flag indicating if FRL (Fixed Rate Link) mode is enabled
 * @linerate: Line rate in bits per second
 * @is_tx: Direction flag (0 for RX, 1 for TX)
 * @lanes: Number of lanes for the configuration
 *
 * This function configures the TI TMDS1204 TX device based on the specified
 * line rate and protocol type. It selects appropriate register settings
 * based on FRL mode, line rate, lane count, and board type.
 *
 * Return: 0 on success, positive value on initialization, negative error code on failure
 */
int ti_tmds1204tx_linerate_conf(u8 is_frl, u64 linerate, u8 is_tx, u8 lanes)
{
	u32 linerate_mbps;
	u32 i = 0;
	u16 dev_type = 0xffff;
	int ret = 1;
	u8 revision = 1;
	u8 board_type;

	/* Convert line rate from bps to Mbps for easier comparison */
	linerate_mbps = (u32)((u64)linerate / 1000000);
	dev_dbg(&txdata->client->dev, "linerate %llu lineratembps %u lanes %d\n\r",
		linerate, linerate_mbps, lanes);

	/* Select device configuration based on revision and protocol type */
	/* TX */
	switch (revision) {
	case 1:
		/* Check if FRL (Fixed Rate Link) mode is enabled */
		if (is_frl == 1) {
			/* Select FRL configuration based on line rate */
			if (linerate_mbps == 12000) {
				dev_type = TX_TI_FRL_12G_R1;
			} else if (linerate_mbps == 10000) {
				dev_type = TX_TI_FRL_10G_R1;
			} else if (linerate_mbps == 8000) {
				dev_type = TX_TI_FRL_8G_R1;
			} else if (linerate_mbps == 6000) {
				/* 6G FRL has different configs for 3 vs 4 lanes */
				if (lanes == 4)
					dev_type = TX_TI_FRL_6G_4_R1;
				else
					dev_type = TX_TI_FRL_6G_3_R1;
			} else if (linerate_mbps == 3000) {
				dev_type = TX_TI_FRL_3G_R1;
			}
		} else {
			/* Select TMDS configuration based on line rate ranges */
			if (linerate_mbps <= 1650)
				dev_type = TX_TI_TMDS_14_L_R1;
			else if (linerate_mbps > 1650 &&
				 linerate_mbps <= 3400)
				dev_type = TX_TI_TMDS_14_H_R1;
			else
				dev_type = TX_TI_TMDS_20_R1;
		}
		break;
	default:
		break;
	}

	/* Get board type for board-specific configurations */
	board_type = xfmcdev_board_type();
	i = 0;
	while (i < ARRAY_SIZE(ti_tmds1204tx_regs) &&
	       dev_type != ti_tmds1204tx_regs[i].dev_type)
		i++;

	/* Program all registers for the selected device configuration */
	while (i < ARRAY_SIZE(ti_tmds1204tx_regs) &&
	       dev_type == ti_tmds1204tx_regs[i].dev_type) {
		ret = ti_tmds1204tx_write_reg(txdata, ti_tmds1204tx_regs[i].addr,
					      ti_tmds1204tx_regs[i].val);

		if (ret)
			return ret;
		i++;
	}

	/* Apply board-specific register overrides for 12G FRL mode */
	if (dev_type == TX_TI_FRL_12G_R1 &&
	    (board_type == BASE_BOARD_ZCU102 || board_type == BASE_BOARD_ZCU106)) {
		/* Configure specific registers for ZCU102 and ZCU106 boards in 12G FRL mode */
		ret = ti_tmds1204tx_write_reg(txdata, 0x12, 0x02);
		ret = ti_tmds1204tx_write_reg(txdata, 0x14, 0x02);
		ret = ti_tmds1204tx_write_reg(txdata, 0x16, 0x02);
		ret = ti_tmds1204tx_write_reg(txdata, 0x18, 0x02);
	}

	dev_info(&txdata->client->dev, "linerate conf done\n\r");
	return ret;
}
EXPORT_SYMBOL_GPL(ti_tmds1204tx_linerate_conf);

static int ti_tmds1204tx_init(struct ti_tmds1204tx *priv, u8 revision, u8 is_tx)
{
	int ret = 1;
	u16 dev_type = 0xffff;
	u32 i = 0;

	switch (revision) {
	case 1:
		dev_type = TX_TI_R1_INIT;
		break;
	default:
		break;
	}

	while (dev_type != ti_tmds1204tx_regs[i].dev_type) {
		i++;
		if (i > sizeof(ti_tmds1204tx_regs))
			break;
	}

	while (dev_type == ti_tmds1204tx_regs[i].dev_type) {
		ret = ti_tmds1204tx_write_reg(priv, ti_tmds1204tx_regs[i].addr,
					      ti_tmds1204tx_regs[i].val);

		if (ret)
			return ret;

		i++;
	}

	dev_dbg(&priv->client->dev, "i2c write OK!!");
	return ret;
}

/*
 * ti_tmds1204tx_probe - Probe function for TI TMDS1204 retimer device
 * @client: Pointer to I2C client device
 *
 * This function initializes the TI TMDS1204 retimer device for HDMI transmission.
 * It performs the following operations:
 * - Allocates memory for the device structure
 * - Initializes the I2C client and mutex for thread-safe operations
 * - Sets up regmap for I2C register communication
 * - Configures the device with default register values for revision 1
 * - Sets client data for later retrieval in other driver functions
 *
 * The probe function is called when the I2C subsystem detects a matching
 * device based on the compatible string "ti_tmds1204,ti_tmds1204-tx" in the
 * device tree. This driver supports TI TMDS1204 retimer chip for signal
 * conditioning and retiming of HDMI/DisplayPort signals.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ti_tmds1204tx_probe(struct i2c_client *client)
{
	int ret;

	/* initialize ti_tmds1204 */
	txdata = devm_kzalloc(&client->dev, sizeof(*txdata), GFP_KERNEL);
	if (!txdata)
		return -ENOMEM;

	txdata->client = client;
	mutex_init(&txdata->lock);

	/* initialize regmap */
	txdata->regmap = devm_regmap_init_i2c(client, &ti_tmds1204tx_regmap_config);
	if (IS_ERR(txdata->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(txdata->regmap));
		ret = -ENODEV;
		goto err_regmap;
	}

	i2c_set_clientdata(client, txdata);

	dev_dbg(&client->dev, "init ti_tmds1204-tx\n");
	ret = ti_tmds1204tx_init(txdata, 1, true);
	if (ret) {
		dev_err(&client->dev, "failed to init ti_tmds1204-tx\n");
		return ret;
	}
	dev_dbg(&client->dev, "TI TMDS1204 TX Driver Probed!!\n");
	return 0;

err_regmap:
	mutex_destroy(&txdata->lock);
	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
static int ti_tmds1204tx_remove(struct i2c_client *client)
{
	return 0;
}
#else
static void ti_tmds1204tx_remove(struct i2c_client *client)
{
	return;
}
#endif

static const struct of_device_id ti_tmds1204tx_of_id_table[] = {
	{ .compatible = "ti_tmds1204,ti_tmds1204-tx" },
	{ }
};
MODULE_DEVICE_TABLE(of, ti_tmds1204tx_of_id_table);

static const struct i2c_device_id ti_tmds1204tx_id[] = {
	{ "ti_tmds1204tx", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ti_tmds1204tx_id);

static struct i2c_driver ti_tmds1204tx_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table	= ti_tmds1204tx_of_id_table,
	},
	.probe		= ti_tmds1204tx_probe,
	.remove		= ti_tmds1204tx_remove,
	.id_table	= ti_tmds1204tx_id,
};

void ti_tmds1204tx_exit(void)
{
	i2c_del_driver(&ti_tmds1204tx_i2c_driver);
}
EXPORT_SYMBOL_GPL(ti_tmds1204tx_exit);

/*
 * ti_tmds1204tx_entry - Entry point function for TI TMDS1204 TX driver
 *
 * This function initializes and registers the TI TMDS1204 TX I2C driver.
 * It checks for device tree compatibility and registers the driver with
 * the I2C subsystem.
 *
 * Return: 0 on success, negative error code on failure
 */
int ti_tmds1204tx_entry(void)
{
	struct device_node *node;

	node = of_find_matching_node(NULL, ti_tmds1204tx_of_id_table);
	if (!node) {
		pr_err("TI_TMDS1204_TX: Compatible DT node 'ti_tmds1204,ti_tmds1204-tx' not found.\n");
		return -ENODEV;
	}
	of_node_put(node);  // Safely release reference to avoid memory leak
	return i2c_add_driver(&ti_tmds1204tx_i2c_driver);
}
EXPORT_SYMBOL_GPL(ti_tmds1204tx_entry);

MODULE_DESCRIPTION("TI TMDS1204 retimer chip driver");
MODULE_LICENSE("GPL v2");
