// SPDX-License-Identifier: GPL-2.0
/*
 * FMC74 Expander driver
 *
 * Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.
 *
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/version.h>

void fmc74_exit(void);
int fmc74_entry(void);

static const struct regmap_config fmc74_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
};

/*
 * struct fmc74 - fmc74 device structure
 * @client: Pointer to I2C client
 * @regmap: Pointer to regmap structure
 * @lock: Mutex structure
 * @mode_index: Resolution mode index
 */
struct fmcs74 {
	struct i2c_client *client;
	struct regmap *regmap;

	/* mutex for serializing operations */
	struct mutex lock;
	u32 mode_index;
};

struct fmcs74 *fmc74;

static inline int fmc74_read_reg(struct fmcs74 *priv, u16 addr, u8 *val)
{
	int err;

	err = regmap_read(priv->regmap, addr, (unsigned int *)val);
	if (err)
		dev_dbg(&priv->client->dev, "fmc74 :regmap_read failed\n");
	return err;
}

static inline int fmc74_write_reg(struct fmcs74 *priv, u16 addr, u8 val)
{
	int err;

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		dev_dbg(&priv->client->dev, "fmc74 :regmap_write failed\n");
	return err;
}

static const struct of_device_id fmc74_of_id_table[] = {
	{ .compatible = "expander-fmc74" },
	{ }
};
MODULE_DEVICE_TABLE(of, fmc74_of_id_table);

static const struct i2c_device_id fmc74_id[] = {
	{ "FMC74", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fmc74_id);

static int fmc74_probe(struct i2c_client *client)
{
	int ret;

	/* initialize fmc74 */
	fmc74 = devm_kzalloc(&client->dev, sizeof(*fmc74), GFP_KERNEL);
	if (!fmc74)
		return -ENOMEM;

	mutex_init(&fmc74->lock);

	/* initialize regmap */
	fmc74->regmap = devm_regmap_init_i2c(client, &fmc74_regmap_config);
	if (IS_ERR(fmc74->regmap)) {
		dev_err(&client->dev, "fmc74: regmap init failed: %ld\n",
			PTR_ERR(fmc74->regmap));
		ret = -ENODEV;
		goto err_regmap;
	}

	dev_dbg(&client->dev, "select port 7\n");
#if defined BASE_BOARD_VCK190
	fmc74_write_reg(fmc74, 0x0, 0x2);
#elif defined BASE_BOARD_VEK280
	fmc74_write_reg(fmc74, 0x0, 0x20);
#else
	fmc74_write_reg(fmc74, 0x0, 0x80);
#endif

	return 0;

err_regmap:
	mutex_destroy(&fmc74->lock);
	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
static int fmc74_remove(struct i2c_client *client)
{
	return 0;
}
#else
static void fmc74_remove(struct i2c_client *client)
{
	return;
}
#endif

static struct i2c_driver fmc74_i2c_driver = {
	.driver = {
		.name	= "fmc74",
		.of_match_table	= fmc74_of_id_table,
	},
	.probe		= fmc74_probe,
	.remove		= fmc74_remove,
	.id_table	= fmc74_id,
};

void fmc74_exit(void)
{
	i2c_del_driver(&fmc74_i2c_driver);
}
EXPORT_SYMBOL_GPL(fmc74_exit);

int fmc74_entry(void)
{
	return i2c_add_driver(&fmc74_i2c_driver);
}
EXPORT_SYMBOL_GPL(fmc74_entry);

MODULE_DESCRIPTION("FMC74 Expander driver");
MODULE_LICENSE("GPL v2");
