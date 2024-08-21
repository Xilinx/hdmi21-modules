// SPDX-License-Identifier: GPL-2.0
/*
 * FMC Expander driver
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

void fmc_exit(void);
int fmc_entry(void);

struct fmcs *fmc;

static const struct regmap_config fmc_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

/*
 * struct fmc - fmc device structure
 * @client: Pointer to I2C client
 * @regmap: Pointer to regmap structure
 * @lock: Mutex structure
 * @mode_index: Resolution mode index
 */
struct fmcs {
	struct i2c_client *client;
	struct regmap *regmap;
	/* mutex for serializing operations */
	struct mutex lock;
	u32 mode_index;
};

static inline void msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base * 1000, delay_base * 1000 + 500);
}

static inline int fmc_read_reg(struct fmcs *priv, u16 addr, u8 *val)
{
	int err;

	err = regmap_read(priv->regmap, addr, (unsigned int *)val);
	if (err)
		dev_dbg(&priv->client->dev, "fmc :regmap_read failed\n");
	return err;
}

static inline int fmc_write_reg(struct fmcs *priv, u16 addr, u8 val)
{
	int err;

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		dev_dbg(&priv->client->dev, "fmc :regmap_write failed\n");
	return err;
}

static const struct of_device_id fmc_of_id_table[] = {
	{ .compatible = "expander-fmc" },
	{ }
};
MODULE_DEVICE_TABLE(of, fmc_of_id_table);

static const struct i2c_device_id fmc_id[] = {
	{ "FMC", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fmc_id);

static int fmc_probe(struct i2c_client *client)
{
	int ret;

	printk("%s\n",__func__);
	fmc = devm_kzalloc(&client->dev, sizeof(*fmc), GFP_KERNEL);
	if (!fmc)
		return -ENOMEM;

	mutex_init(&fmc->lock);

	fmc->regmap = devm_regmap_init_i2c(client, &fmc_regmap_config);
	if (IS_ERR(fmc->regmap)) {
		dev_err(&client->dev,
			"Failed to register i2c regmap %d\n",
			(int)PTR_ERR(fmc->regmap));
		ret = PTR_ERR(fmc->regmap);
		goto err_regmap;
	}

	/* select HPC connector */
	ret = fmc_write_reg(fmc, 0x0, 0x01);
	if (ret) {
		dev_err(&client->dev," Failed to select HDP0 port\n");
		return ret;
	}

	printk("%s done\n",__func__);
	return 0;

err_regmap:
	mutex_destroy(&fmc->lock);
	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
static int fmc_remove(struct i2c_client *client)
{
	return 0;
}
#else
static void fmc_remove(struct i2c_client *client)
{
	return;
}
#endif

static struct i2c_driver fmc_i2c_driver = {
	.driver = {
		.name	= "fmc",
		.of_match_table	= fmc_of_id_table,
	},
	.probe		= fmc_probe,
	.remove		= fmc_remove,
	.id_table	= fmc_id,
};

void fmc_exit(void)
{
	i2c_del_driver(&fmc_i2c_driver);
}
EXPORT_SYMBOL_GPL(fmc_exit);

int fmc_entry(void)
{
	return i2c_add_driver(&fmc_i2c_driver);
}
EXPORT_SYMBOL_GPL(fmc_entry);

MODULE_DESCRIPTION("FMC Expander driver");
MODULE_LICENSE("GPL v2");
