// SPDX-License-Identifier: GPL-2.0
/*
 * IDT Expander driver
 *
 * Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.
 *
 * This is a common clock framework driver for idt_8T49N24x clock provider.
 * This will generate TMDS clock for HDMI 2.1 TX subsystem based on the
 * requested resolution.
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

#define IDT_8T49N24X_REVID 0x0    		 //!< Device Revision
#define IDT_8T49N24X_DEVID 0x0607 		 //!< Device ID Code

#define IDT_8T49N24X_XTAL_FREQ 40000000  //!< The freq of the crystal in Hz
#define IDT_8T49N24X_FVCO_MAX 4000000000 //!< Max VCO Operating Freq in Hz
#define IDT_8T49N24X_FVCO_MIN 3000000000 //!< Min VCO Operating Freq in Hz
#define IDT_8T49N24X_FOUT_MAX 400000000  //!< Max Output Freq in Hz
#define IDT_8T49N24X_FOUT_MIN      8000  //!< Min Output Freq in Hz
#define IDT_8T49N24X_FIN_MAX 875000000   //!< Max input Freq in Hz
#define IDT_8T49N24X_FIN_MIN      8000   //!< Min input Freq in Hz
#define IDT_8T49N24X_FPD_MAX 128000      //!< Max Phase Detector Freq in Hz
#define IDT_8T49N24X_FPD_MIN   8000      //!< Min Phase Detector Freq in Hz
#define IDT_8T49N24X_P_MAX 4194304  /* pow(2,22) */  //!< Max P div value
#define IDT_8T49N24X_M_MAX 16777216 /* pow(2,24) */  //!< Max M mult value

#define DRIVER_NAME "idt"

void idt_exit(void);
int idt_entry(void);

struct idt_settings {
	u32 dsm_frac;
	u32 m1_x;
	u32 pre_x;
	u32 los_x;
	u32 n_qx;
	u32 nfrac_qx;
	u16 ns2_qx;
	u16 dsm_int;
	u8  ns1_qx;
	
};

static const struct regmap_config idt_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

/*
 * struct idt - idt device structure
 * @client: Pointer to I2C client
 * @ctrls: idt control structure
 * @regmap: Pointer to regmap structure
 * @lock: Mutex structure
 * @mode_index: Resolution mode index
 */
struct idts {
	struct clk_hw hw;
	struct i2c_client *client;
	struct regmap *regmap;
	struct mutex lock; /* mutex lock for operations */
	u32 mode_index;
};

int set_clock(struct idts *idt, u32 freq_in, u32 freq_out);

#define to_idts(_hw)	container_of(_hw, struct idts, hw)

static inline int idt_read_reg(struct idts *priv, u8 addr, u8 *val)
{
	int err = 0;

	err = regmap_read(priv->regmap, addr, (unsigned int *)val);
	if (err)
		dev_dbg(&priv->client->dev,
			"i2c read failed, addr = %x\n", addr);
	return err;
}

static inline int idt_write_reg(struct idts *priv, u16 addr, u8 val)
{
	int err = 0;

	err = regmap_write(priv->regmap, addr, val);
	if (err) {
		dev_dbg(&priv->client->dev,
			"i2c write failed, addr = %x\n", addr);
	}

	return err;
}

static int idt_get_int_divtable(int freq_out, int *divtbl, u8 bypass)
{
	int ns1_opts[4] = {1,4,5,6};
	int index;
	int ns2_min = 1;
	int ns2_max = 1;
	
	int ns2_tmp;
	int outdiv_tmp;
	u32 vco_tmp;
	int outdiv_min;
	int outdiv_max;
	int i;
	int cnt = 0;
	int *divtbl_ptr = divtbl;
	/* ceil(IDT_8T49N24X_fvco_MIN/freq_out) */
	outdiv_min = (IDT_8T49N24X_FVCO_MIN/freq_out) +
					(((IDT_8T49N24X_FVCO_MIN % freq_out)==0) ? 0 : 1);
	/* (int)floor(IDT_8T49N24X_fvco_MAX/freq_out) */
	outdiv_max = (IDT_8T49N24X_FVCO_MAX/freq_out);
	
	if (bypass == true) {
		index = 0;
	}
	else {
		index = 1;
	}
	
	for (i = index; i < (sizeof(ns1_opts)/sizeof(int)); i++) {
		/* This is for the case where we want to bypass NS2 */
		if ((ns1_opts[i] == outdiv_min) || (ns1_opts[i] == outdiv_max)) {
			ns2_min = 0;
			ns2_max = 0;
		}
	}
	
	/* if this test passes, then we know we're not in the bypass case */
	if (ns2_min == 1) {
		/* The last element in the list */
		/* (int)ceil(outdiv_min / ns1_opts[3] / 2) */
		ns2_min = (outdiv_min / ns1_opts[3] / 2) +
				((((outdiv_min / ns1_opts[3]) % 2)==0) ? 0 : 1);
		/* floor(outdiv_max / ns1_opts[index] / 2) */
		ns2_max = (outdiv_max / ns1_opts[index] / 2);
		if (ns2_max == 0)
			/* because we're rounding-down for the max, we may end-up with
			   it being 0, in which case we need to make it 1 */
			ns2_max = 1; 
	}
	 
	ns2_tmp = ns2_min;
	
	while (ns2_tmp <= ns2_max) {
		for (i = index; i < (sizeof(ns1_opts)/sizeof(int)); i++) {
			if (ns2_tmp == 0) {
				outdiv_tmp = ns1_opts[i];
			}
			else {
				outdiv_tmp = ns1_opts[i] * ns2_tmp * 2;
			}
			
			vco_tmp = freq_out * outdiv_tmp;
			
			if ((vco_tmp <= IDT_8T49N24X_FVCO_MAX) &&
			    (vco_tmp >= IDT_8T49N24X_FVCO_MIN)) {
				*divtbl_ptr = outdiv_tmp;
				cnt++;
				divtbl_ptr++;
			}
		}
		ns2_tmp++;
	}
	
	return cnt;
}

static int idt_cal_settings(int freq_in, int freq_out, struct idt_settings *settings)
{
	int divtbl[20];
	int divtbl_cnt;
	int max_div = 0;
	unsigned int fvco;
	int ns1;
	int ns2;
	int ns1_ratio;
	int ns2_ratio;
	unsigned int UpperFBDiv, UpperFBDiv_rem;
	int dsm_int;
	u64 dsm_frac;
	int los;
	int i;
	u32 n_q2 = 0;
	u32 nfrac_q2 = 0;
	u64 m1 = 0;
	int p_min;
	unsigned int frac_numerator;
	int m1_default;
	int p_default;
	int error_tmp = 999999;
	int error = 99999999;

	int count = 0;

	/* Get the valid integer dividers */
	divtbl_cnt = idt_get_int_divtable(freq_out, divtbl, false);
	
	/* Find the highest divider */
	for (i = 0; i < divtbl_cnt; i++) {
		if (max_div < divtbl[i]) {
			max_div = divtbl[i];
		}
	}
	fvco = freq_out*max_div;
	
	/***************************************************/
	/* INTEGER DIVIDER: Determine NS1 register setting */
	/***************************************************/
#if 0	
	/* Only use the divide-by-1 option for really small divide ratios
	 * note that this option will never be on the list for the
	 * Q0 - Q3 dividers
	 */
	if (max_div < 4) { 
		
	}
#endif	
	/* Make sure we can divide the ratio by 4 in NS1 and by 1 or an
	 * even number in NS2
	 */
	if ((max_div == 4) ||
	    (max_div % 8 == 0)) { 
		/* Divide by 4 register selection */
		ns1 = 2;
	}
	
	/* Make sure we can divide the ratio by 5 in NS1 and by 1 or
	 * an even number in NS2
	 */
	if ((max_div == 5) ||
	    (max_div % 10 == 0)) {
		/* Divide by 5 register selection */
		ns1 = 0;
	}
	
	/* Make sure we can divide the ratio by 6 in NS1 and by 1 or
	 * an even number in NS2
	 */
	if ((max_div == 6) ||
	    (max_div % 12 == 0)) {
		/* Divide by 6 register setting */
		ns1 = 1;
	}
	
	/***************************************************/
	/* INTEGER DIVIDER: Determine NS2 register setting */
	/***************************************************/
	
	switch (ns1) {
		case (0) :
			ns1_ratio = 5;
		break;
		
		case (1) :
			ns1_ratio = 6;
		break;
		
		case (2) :
			ns1_ratio = 4;
		break;
		
		case (3) :
			/* This is the bypass (divide-by-1) option */
			ns1_ratio = 1;
		break;
		
		default :
			ns1_ratio = 6;
		break;
	}
	
	/* floor(max_div / ns1_ratio) */
	ns2_ratio = (max_div / ns1_ratio);
	
	/* floor(ns2_ratio/2) */
	ns2 = (ns2_ratio/2);
	
	if (max_div & 1)
	{
		frac_numerator = (268435456 >> 1);
	}
	else
	{
		frac_numerator = 0;
	}
	/* This is the case where the fractional portion is 0.
	 * Due to precision limitations, sometimes fractional portion of the
	 * Effective divider gets rounded to 1.  This checks for that condition
	 */
	if (!(max_div & 1))
	{
		/* n_q2 = (int)round(FracDiv / 2.0); */
		n_q2 = max_div >> 1;
		nfrac_q2 = 0;
	}
	else
	{
		/* n_q2 = (int)floor(FracDiv / 2.0); */
		n_q2 = ((max_div+1)>>1);
		nfrac_q2 = frac_numerator;
	}

	/*****************************************************/
	/* Calculate the Upper Loop Feedback divider setting */
	/*****************************************************/
	
	UpperFBDiv = (fvco) / (2*IDT_8T49N24X_XTAL_FREQ);
	UpperFBDiv_rem = fvco % (2 * IDT_8T49N24X_XTAL_FREQ);

	/* dsm_int = (int)floor(UpperFBDiv); */
	dsm_int = (int)(UpperFBDiv);
	
	/*dsm_frac =
	 * 			(int)round((UpperFBDiv - floor(UpperFBDiv))*pow(2,21));
	 */
	//dsm_frac = (int)(((UpperFBDiv - (int)UpperFBDiv)*2097152) + 1/2);
	dsm_frac = ((u64)UpperFBDiv_rem * 2048) + (78125>>1);
	dsm_frac = dsm_frac/ (78125);
	
	/*****************************************************/
	/* Calculate the Lower Loop Feedback divider and
	 * input Divider
	 *****************************************************/
	
//	Ratio = fvco/freq_in;
	
	p_min = (int)freq_in/IDT_8T49N24X_FPD_MAX;
	
	/* This m1 divider sets the input PFD frequency at 128KHz, the set max */
	/* int M1Min = (int)(fvco/IDT_8T49N24X_FPD_MAX); */


	/* Start from lowest divider and iterate until 0 error is found
	 * or the divider limit is exhausted.
	 */
	/* Keep the setting with the lowest error */
	for (i = p_min; i <= IDT_8T49N24X_P_MAX; i++) {
		/* m1 = (int)round(i*Ratio); */
//		m1 = (int)(i*Ratio +  1/2);
		m1 = (u64)fvco *i;
		m1 += (freq_in >> 1);
		m1 /= freq_in;
		count++;
		if (m1 < IDT_8T49N24X_M_MAX) {
			u64 temp = ((u64)fvco*i - (u64)m1*freq_in)*1000000;
			u64 temp1 = (u64)i*freq_in/1000;
			temp = temp / temp1;
			error_tmp = (int)temp;
//			error_tmp = (int)(Ratio*1000000000 - (m1*1000000000 / i));

			if (abs(error_tmp) < error || error_tmp == 0) {
				error = abs(error_tmp);
				m1_default = m1;
				p_default = i;

				if (error_tmp == 0)
					break;
			}
		}
		else {
			break;
		}
	}
	
	/* Calculate los */
	los = fvco / 8 / freq_in; 
	los = los + 3;
	if (los < 6)
		los = 6;

	/* Copy registers */
	settings->ns1_qx = ns1;
	settings->ns2_qx = ns2;
	
	settings->n_qx = n_q2;
	settings->nfrac_qx = nfrac_q2;

	settings->dsm_int = dsm_int;
	settings->dsm_frac = dsm_frac;
	settings->m1_x = m1_default;
	settings->pre_x = p_default;
	settings->los_x = los;

	return 0;
}

static int idt_pre_div(struct idts *idt, u32 val, u8 input)
{
	int ret;
	u8 data;
	u16 addr;

	if (input == 1)
		addr = 0x000e;
	else
		addr = 0x000b;

	/* PREx[20:16] */
	data = (val >> 16) & 0x1f; 
	ret = idt_write_reg(idt, addr, data);

	/* PREx[15:8] */
	data = (val >> 8); 
	ret = idt_write_reg(idt, addr+1, data);

	/* PREx[7:0] */
	data = (val & 0xff); 
	ret = idt_write_reg(idt, addr+2, data);

	return ret;
}

static int idt_m1_feedback(struct idts *idt, u32 val, u8 input)
{
	int ret;
	u8 data;
	u16 addr;

	if (input == 1)
		addr = 0x0011;
	else
		addr = 0x0014;

	/* M1x[23:16] */
	data = (val >> 16); 
	ret = idt_write_reg(idt, addr, data);

	/* m1x[15:8] */
	data = (val >> 8); 
	ret = idt_write_reg(idt, addr+1, data);

	/* M1x[7:0] */
	data = (val & 0xff); 
	ret = idt_write_reg(idt, addr+2, data);

	return ret;
}
static int idt_dsm_int(struct idts *idt, u16 val)
{
	int ret;
	u8 data;

	/* dsm_int[8] */
	data = (val >> 8) & 0x01; 
	ret = idt_write_reg(idt, 0x0025, data);

	/* dsm_int[7:0] */
	data = (val & 0xff); 
	ret = idt_write_reg(idt, 0x0026, data);

	return ret;
}

static int idt_dsm_frac(struct idts *idt, u32 val)
{
	int ret;
	u8 data;

	/* dsm_frac[20:16] */
	data = (val >> 16) & 0x1f; 
	ret = idt_write_reg(idt, 0x0028, data);

	/* dsm_frac[15:8] */
	data = (val >> 8); 
	ret = idt_write_reg(idt, 0x0029, data);

	/* dsm_frac[7:0] */
	data = (val & 0xff); 
	ret = idt_write_reg(idt, 0x002a, data);

	return ret;
}

static int idt_outdiv_int(struct idts *idt, u32 val, u8 output)
{
	int ret;
	u8 data;
	u16 addr;

	switch (output) {
		case 0 : addr = 0x003f;
			break;
		case 1 : addr = 0x0042;
			break;
		case 2 : addr = 0x0045;
			break;
		case 3 : addr = 0x0048;
			break;
	}
	
	/* N_Qm[17:16] */
	data = (val >> 16) & 0x03; 
	ret = idt_write_reg(idt, addr, data);

	/* N_Qm[15:8] */
	data = (val >> 8); 
	ret = idt_write_reg(idt, addr+1, data);

	/* N_Qm[7:0] */
	data = (val & 0xff); 
	ret = idt_write_reg(idt, addr+2, data);

	return ret;
}
static int idt_outdiv_frac(struct idts *idt, u32 val, u8 output)
{
	int ret;
	u8 data;
	u16 addr;

	switch (output) {
		case 0 : addr = 0x0000;
			break;
		case 1 : addr = 0x0057;
			break;
		case 2 : addr = 0x005b;
			break;
		case 3 : addr = 0x005f;
			break;
	}
	
	/* NFRAC_Qm[27:24] */
	data = (val >> 24) & 0x0f; 
	ret = idt_write_reg(idt, addr, data);

	/* NFRAC_Qm[23:16] */
	data = (val >> 16); 
	ret = idt_write_reg(idt, addr+1, data);

	/* NFRAC_Qm[15:8] */
	data = (val >> 8); 
	ret = idt_write_reg(idt, addr+2, data);

	/* NFRAC_Qm[7:0] */
	data = (val & 0xff); 
	ret = idt_write_reg(idt, addr+3, data);

	return ret;
}
static int idt_modify_reg(struct idts *idt, u16 addr, u8 val, u8 mask)
{
	unsigned int data;
	int ret;

	/* Read data */
	regmap_read(idt->regmap, addr, &data);
	/* Clear masked bits */
	data &= ~mask; 

	/* Update */
	data |= (val & mask);

	/* Write data */
	ret = idt_write_reg(idt, addr, data);

	return ret;
}

static int idt_set_mode(struct idts *idt, u8 synthesizer)
{
	int ret;
	u8 val;
	u8 mask;

	/* Digital PLL: State[1:0] */
	if (synthesizer) {
		val = 0x01;		/* Force FREERUN */
		val |= (1<<4);	/* Disable reference input 0 */
		val |= (1<<5);	/* Disable reference input 1 */
	}

	else {
		val = 0x00;		/* Run automatically */
		val |= (1<<5);	/* Disable reference input 1 */
	}
	mask = 0x33;
	ret = idt_modify_reg(idt, 0x000a, val, mask);
	/* Analog PLL: SYN_MODE */
	if (synthesizer) {
		val = (1<<3);		/* synthesizer mode */
	}

	else {
		val = 0x00;		/* Jitter attenuator mode */
	}
	mask = (1<<3);
	ret = idt_modify_reg(idt, 0x0069, val, mask);

	return ret;
}
static int idt_in_monitor_ctrl(struct idts *idt, u32 val, u8 input)
{
	int ret;
	u8 data;
	u16 addr;

	if (input == 1)
		addr = 0x0074;
	else
		addr = 0x0071;

	/* losx[16] */
	data = (val >> 16) & 0x1; 
	ret = idt_write_reg(idt, addr, data);

	/* losx[15:8] */
	data = (val >> 8); 
	ret = idt_write_reg(idt, addr+1, data);

	/* losx[7:0] */
	data = (val & 0xff); 
	ret = idt_write_reg(idt, addr+2, data);

	return ret;
}

static int idt_ref_input(struct idts *idt, u8 input, u8 enable)
{
	int ret;
	u8 val;
	u8 mask;
	u8 shift;

	if (input == 1) {
		shift = 5;
	}

	else {
		shift = 4;
	}

	/* enable */
	if (enable) {
		val = 0x00;		/* enable */
	}

	/* Disable */
	else {
		val = (1<<shift);	/* Disable reference input  */
	}
	mask = (1<<shift);
	ret = idt_modify_reg(idt, 0x000a, val, mask);

	return ret;
}

int set_clock(struct idts *idt, u32 freq_in, u32 freq_out)
{
	int ret;
	struct idt_settings settings;

	if ((freq_in < IDT_8T49N24X_FIN_MIN) &&
	   (freq_in > IDT_8T49N24X_FIN_MAX)) {
		dev_dbg(&idt->client->dev, "input frequency is not in range \n\r");
		return 1;
	}
	
	if ((freq_out < IDT_8T49N24X_FOUT_MIN) &&
		(freq_out > IDT_8T49N24X_FOUT_MAX)) {
		dev_dbg(&idt->client->dev, "output frequency is not in range \n\r");
		return 1;
	}
	
	/* Calculate settings */
	idt_cal_settings(freq_in, freq_out, &settings);

	/* Disable DPLL and APLL calibration */
	idt_write_reg(idt, 0x0070, 0x05); 

	/* Free running mode */
	/* Disable reference clock input 0 */
	ret = idt_ref_input(idt, 0, false);

	/* Disable reference clock input 1 */
	ret = idt_ref_input(idt, 1, false);

	/* Set synthesizer mode */
	ret = idt_set_mode(idt, true);

	/* Pre-divider input 0 */
	ret = idt_pre_div(idt, settings.pre_x, 0);
	/* Pre-divider input 1 */
	ret = idt_pre_div(idt, settings.pre_x, 1);
	/* M1 feedback input 0 */
	ret = idt_m1_feedback(idt, settings.m1_x, 0);
	/* M1 feedback input 1 */
	ret = idt_m1_feedback(idt, settings.m1_x, 1);

	/* DSM integer */
	ret = idt_dsm_int(idt, settings.dsm_int);

	/* DSM fractional */
	ret = idt_dsm_frac(idt, settings.dsm_frac);

	/* output divider integer output 2 */
	ret = idt_outdiv_int(idt, settings.n_qx, 2);

	/* output divider integer output 3 */
	ret = idt_outdiv_int(idt, settings.n_qx, 3);

	/* output divider fractional output 2 */
	ret = idt_outdiv_frac(idt, settings.nfrac_qx, 2);

	/* output divider fractional output 3 */
	ret = idt_outdiv_frac(idt, settings.nfrac_qx, 3);

	/* input monitor control 0 */
	ret = idt_in_monitor_ctrl(idt, settings.los_x, 0);

	/* input monitor control 1 */
	ret = idt_in_monitor_ctrl(idt, settings.los_x, 1);

	/* enable DPLL and APLL calibration */
	ret = idt_write_reg(idt, 0x0070, 0x00); 

	return ret;
}

static unsigned long idt_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct idts *idt = to_idts(hw);

	dev_dbg(&idt->client->dev, "%s \n",__func__);

	return 0;
}

static long idt_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	struct idts *idt = to_idts(hw);

	dev_dbg(&idt->client->dev, "%s \n",__func__);

	return rate;
}

static int idt_set_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	struct idts *idt = to_idts(hw);
	
	set_clock(idt, IDT_8T49N24X_XTAL_FREQ,rate);
	
	return 0;
}

static const struct clk_ops idt_clk_ops = {
	.recalc_rate = idt_recalc_rate,
	.round_rate = idt_round_rate,
	.set_rate = idt_set_rate,
};

static const struct of_device_id idt_of_id_table[] = {
	{ .compatible = "idt,idt8t49" },
	{ }
};
MODULE_DEVICE_TABLE(of, idt_of_id_table);

static const struct i2c_device_id idt_id[] = {
	{ "IDT", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, idt_id);

/*
	This configuration was created with the IDT timing commander.
	It configures the clock device is Jitter Attenuator mode.
	It produces a 148.5 MHz clock on outputs Q2 and Q3 from an incoming
	148.5 MHz clock.
*/ 
static const u8 IDT_8T49N24x_Config_JA[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xEF, 0x00, 0x03, 0x00, 0x20, 0x00,
	0x04, 0x89, 0x00, 0x00, 0x01, 0x00, 0x63, 0xC6, 0x07, 0x00, 0x00, 0x77,
	0x6D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
	0x3F, 0x00, 0x28, 0x00, 0x1A, 0xCC, 0xCD, 0x00, 0x01, 0x00, 0x00, 0xD0,
	0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x0C, 0x00, 0x00,
	0x00, 0x44, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B,
	0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0x02, 0x2B, 0x20,
	0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static int idt_init(struct idts *idt)
{
	u32 Index;
	idt_write_reg(idt, 0x0070, 0x05);
	/* The configuration is started from address 0x08 */
	for (Index=8; Index<sizeof(IDT_8T49N24x_Config_JA); Index++) {
		/* Skip address 0x70 */
		/* Address 0x70 enables the DPLL and APLL calibration */
		if (Index != 0x070) {
			idt_write_reg(idt, Index, IDT_8T49N24x_Config_JA[Index]);
		}
	}
	idt_write_reg(idt, 0x0070, 0x00);

	return 0;	
}

static int idt_probe(struct i2c_client *client)
{
	struct idts *data;
	struct clk_init_data init;
	u32 initial_fout;
	int ret, err;

	/* initialize idt */
	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	init.ops = &idt_clk_ops;
	init.flags = 0;
	init.num_parents = 0;
	data->hw.init = &init;
	data->client = client;

	mutex_init(&data->lock);

	if (of_property_read_string(client->dev.of_node, "clock-output-names",
			&init.name))
		init.name = client->dev.of_node->name;

	/* initialize regmap */
	data->regmap = devm_regmap_init_i2c(client, &idt_regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(data->regmap));
		ret = -ENODEV;
		goto err_regmap;
	}

	i2c_set_clientdata(client, data);

	err = devm_clk_hw_register(&client->dev, &data->hw);
	if (err) {
		dev_err(&client->dev, "clock registration failed\n");
		return err;
	}

	err = of_clk_add_hw_provider(client->dev.of_node, of_clk_hw_simple_get,
				     &data->hw);
	if (err) {
		dev_err(&client->dev, "unable to add clk provider\n");
		return err;
	}

	dev_dbg(&client->dev, "Initialize idt with default values \n");
	idt_init(data);
	dev_dbg(&client->dev, "GPIO LOL ENABLE \n\r");
	idt_write_reg(data, 0x0030, 0x0F);
	idt_write_reg(data, 0x0034, 0x00);
	idt_write_reg(data, 0x0035, 0x00);
	idt_write_reg(data, 0x0036, 0x0F);

	/* Read the requested initial output frequency from device tree */
	if (!of_property_read_u32(client->dev.of_node, "clock-frequency",
				&initial_fout)) {
		err = clk_set_rate(data->hw.clk, initial_fout);
		if (err) {
			of_clk_del_provider(client->dev.of_node);
			return err;
		}
	}

	return 0;

err_regmap:
	mutex_destroy(&data->lock);
	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
static int idt_remove(struct i2c_client *client)
{
	return 0;
}
#else
static void idt_remove(struct i2c_client *client)
{
        return;
}
#endif

static struct i2c_driver idt_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table	= idt_of_id_table,
	},
	.probe		= idt_probe,
	.remove		= idt_remove,
	.id_table	= idt_id,
};

void idt_exit(void)
{
	i2c_del_driver(&idt_i2c_driver);
}
EXPORT_SYMBOL_GPL(idt_exit);

int idt_entry(void)
{
	return i2c_add_driver(&idt_i2c_driver);
}
EXPORT_SYMBOL_GPL(idt_entry);

MODULE_DESCRIPTION("8T49N24x ccf driver");
MODULE_LICENSE("GPL v2");
