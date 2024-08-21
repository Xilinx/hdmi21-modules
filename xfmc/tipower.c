// SPDX-License-Identifier: GPL-2.0
/*
 * TIPOWER clock generator driver
 *
 * Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.
 *
 * This is a clock generator driver for LMK03318 device. This driver programs
 * the LMK03318 chip in probe function of the driver to generate fixed clock.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/version.h>

int tipower_entry(void);
void tipower_exit(void);
int tipower_init(void);

struct reg_8 {
	u16 addr;
	u8 val;
};

static const struct regmap_config tipower_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

/*
 * struct tipower - tipower device structure
 * @client: Pointer to I2C client
 * @ctrls: tipower control structure
 * @regmap: Pointer to regmap structure
 * @lock: Mutex structure
 * @mode_index: Resolution mode index
 */
struct tipowers {
	struct i2c_client *client;
	struct regmap *regmap;

	/* mutex for serializing operations */
	struct mutex lock;
	u32 mode_index;
};

struct tipowers *tipower;

static inline int tipower_read_reg(struct tipowers *priv, u16 addr, u8 *val)
{
	int err;

	err = regmap_read(priv->regmap, addr, (unsigned int *)val);
	if (err)
		dev_dbg(&priv->client->dev, "tipower :regmap_read failed\n");
	return err;
}

static inline int tipower_write_reg(struct tipowers *priv, u16 addr, u8 val)
{
	int err;
	err = regmap_write(priv->regmap, addr, val);
	if (err)
		dev_dbg(&priv->client->dev, "tipower :regmap_write failed\n");
	return err;
}

int tipower_init(void)
{
	int ret = 0;

	ret = tipower_write_reg(tipower, 0x1d, 0x8f);
	if (ret)
		return 1;

	ret = tipower_write_reg(tipower, 0x32, 0x50);
	ret = tipower_write_reg(tipower, 0x38, 0x01);
	ret = tipower_write_reg(tipower, 0x1E, 0x23);
	ret = tipower_write_reg(tipower, 0x1F, 0x00);
	ret = tipower_write_reg(tipower, 0x20, 0x00);
	ret = tipower_write_reg(tipower, 0x22, 0x00);
	ret = tipower_write_reg(tipower, 0x23, 0x00);
	ret = tipower_write_reg(tipower, 0x25, 0x92);
	ret = tipower_write_reg(tipower, 0x27, 0xD2);
	ret = tipower_write_reg(tipower, 0x29, 0x92);
	ret = tipower_write_reg(tipower, 0x2B, 0x00);

	return 0;
}

static const struct of_device_id tipower_of_id_table[] = {
	{ .compatible = "expander-tipower" },
	{ }
};
MODULE_DEVICE_TABLE(of, tipower_of_id_table);

static const struct i2c_device_id tipower_id[] = {
	{ "TIPOWER", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tipower_id);

static int tipower_probe(struct i2c_client *client)
{
	int ret;

	/* initialize tipower */
	tipower = devm_kzalloc(&client->dev, sizeof(*tipower), GFP_KERNEL);
	if (!tipower)
		return -ENOMEM;

	mutex_init(&tipower->lock);

	/* initialize regmap */
	tipower->regmap = devm_regmap_init_i2c(client, &tipower_regmap_config);
	if (IS_ERR(tipower->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(tipower->regmap));
		ret = -ENODEV;
		goto err_regmap;
	}
	dev_dbg(&client->dev, "Initialize ti chip with default values\n");
	tipower_init();

	return 0;

err_regmap:
	mutex_destroy(&tipower->lock);
	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
static int tipower_remove(struct i2c_client *client)
#else
static void tipower_remove(struct i2c_client *client)
#endif
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
	return 0;
#else
	return;
#endif
}

static struct i2c_driver tipower_i2c_driver = {
	.driver = {
		.name	= "tipower",
		.of_match_table	= tipower_of_id_table,
	},
	.probe		= tipower_probe,
	.remove		= tipower_remove,
	.id_table	= tipower_id,
};

void tipower_exit(void)
{
	i2c_del_driver(&tipower_i2c_driver);
}
EXPORT_SYMBOL_GPL(tipower_exit);

int tipower_entry(void)
{
	return i2c_add_driver(&tipower_i2c_driver);
}
EXPORT_SYMBOL_GPL(tipower_entry);

MODULE_DESCRIPTION("TIPOWER Expander driver");
MODULE_LICENSE("GPL v2");
