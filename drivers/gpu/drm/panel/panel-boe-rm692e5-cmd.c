/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
#include "include/panel-boe-rm692e5-cmd.h"
#include "include/panel-boe-rm692e5-cmd-gamma.h"

/* i2c control start */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *_lcm_i2c_client;
/*PRIZE:Added by lvyuanchuan,X9-678,20221230 start*/
#define BLK_LEVEL_OFFSET			(0)
#define BLK_LEVEL_MAP3				(4095)
extern int mtk_drm_esd_check_status(void);
extern void mtk_drm_esd_set_status(int status);
//prize add by wangfei for lcd hardware info 20210726 start
#if defined(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/prize/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif
//prize add by wangfei for lcd hardware info 20210726 end


//prize add by wangfei for HBM 20210906 start
unsigned int bl_level;
//prize add by wangfei for HBM 20210906 end
struct lcm *g_ctx;
//prize add by majiangtao for frequency Select 20230103 start
int g_vrefresh = -1;
//prize add by majiangtao for frequency Select 20230103 end
/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct _lcm_i2c_dev {
	struct i2c_client *client;
};

static const struct of_device_id _lcm_i2c_of_match[] = {
	{
	    .compatible = "mediatek,i2c_lcd_bias",
	},
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = { { LCM_I2C_ID_NAME, 0 },
						    {} };

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	/* .detect		   = _lcm_i2c_detect, */
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},
};

static unsigned int g_current_level = 0;

/*****************************************************************************
 * Function
 *****************************************************************************/

#ifdef VENDOR_EDIT
// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
extern void lcd_queue_load_tp_fw(void);
#endif /*VENDOR_EDIT*/

static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	pr_err("[LCM][I2C] %s\n", __func__);
	pr_err("[LCM][I2C] NT: info==>name=%s addr=0x%x\n", client->name,
		 client->addr);
	_lcm_i2c_client = client;
	return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_err("[LCM][I2C] %s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_debug("ERROR!! _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	pr_err("[LCM][I2C] %s\n", __func__);
	i2c_add_driver(&_lcm_i2c_driver);
	pr_debug("[LCM][I2C] %s success\n", __func__);
	return 0;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_err("[LCM][I2C] %s\n", __func__);
	i2c_del_driver(&_lcm_i2c_driver);
}

module_init(_lcm_i2c_init);
module_exit(_lcm_i2c_exit);
/***********************************/

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	struct gpio_desc *pm_enable_gpio;

	bool prepared;
	bool enabled;

	bool hbm_en;
	bool hbm_wait;
	bool hbm_stat;           //0Î´ÔÚHBM  1ÔÚHBM

	int error;
};

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
static struct regulator *disp_bias_pos;
static struct regulator *disp_bias_neg;


static int lcm_panel_bias_regulator_init(void)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_bias_pos = regulator_get(NULL, "dsv_pos");
	if (IS_ERR(disp_bias_pos)) { /* handle return value */
		ret = PTR_ERR(disp_bias_pos);
		pr_err("get dsv_pos fail, error: %d\n", ret);
		return ret;
	}

	disp_bias_neg = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(disp_bias_neg)) { /* handle return value */
		ret = PTR_ERR(disp_bias_neg);
		pr_err("get dsv_neg fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */

}

static int lcm_panel_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_bias_pos, 5400000, 5400000);
	if (ret < 0)
		pr_err("set voltage disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_set_voltage(disp_bias_neg, 5400000, 5400000);
	if (ret < 0)
		pr_err("set voltage disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		pr_err("enable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		pr_err("enable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

static int lcm_panel_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	ret = regulator_disable(disp_bias_neg);
	if (ret < 0)
		pr_err("disable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_disable(disp_bias_pos);
	if (ret < 0)
		pr_err("disable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}
#endif
/*PRIZE:Added by lvyuanchuan,X9-678,20221230 start*/
static void lcm_pannel_reconfig_blk(struct lcm *ctx)
{
	char bl_tb0[] = {0x51,0x0F,0xFF};
	unsigned int reg_level = 125;
	pr_err("[%s][%d]bl_level:%d , esd:%d \n",__func__,__LINE__,bl_level ,mtk_drm_esd_check_status());
	if(mtk_drm_esd_check_status()){
		/*PRIZE:Added by lvyuanchuan,X9-534,20230103*/
		if(bl_level)
			reg_level = Gamma_to_level[bl_level] + BLK_LEVEL_OFFSET;
		else
			reg_level = 0;
		bl_tb0[1] = (reg_level>>8)&0xf;
		bl_tb0[2] = (reg_level)&0xff;
		lcm_dcs_write(ctx,bl_tb0,ARRAY_SIZE(bl_tb0));
		mtk_drm_esd_set_status(0);
	}
}
/*PRIZE:Added by lvyuanchuan,X9-678,20221230 end*/

static void lcm_panel_init(struct lcm *ctx)
{
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}

	pr_err("gezi----------%s----%d,bl_level %d\n",__func__,__LINE__,bl_level);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	// udelay(250 * 1000);
	mdelay(50);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

//R
//R
	lcm_dcs_write_seq_static(ctx,0xFE,0x38);
	lcm_dcs_write_seq_static(ctx,0x17,0x04);
	lcm_dcs_write_seq_static(ctx,0x18,0x13);

	lcm_dcs_write_seq_static(ctx,0xFE,0xF4);    //Demura
	lcm_dcs_write_seq_static(ctx,0x00,0x77);
	lcm_dcs_write_seq_static(ctx,0x01,0xDF);
	lcm_dcs_write_seq_static(ctx,0x02,0xAD);
	lcm_dcs_write_seq_static(ctx,0x03,0x53);
	lcm_dcs_write_seq_static(ctx,0x04,0x2E);
	lcm_dcs_write_seq_static(ctx,0x05,0xB0);
	lcm_dcs_write_seq_static(ctx,0x06,0xA4);
	lcm_dcs_write_seq_static(ctx,0x07,0x82);
	lcm_dcs_write_seq_static(ctx,0x08,0xCA);
	lcm_dcs_write_seq_static(ctx,0x09,0x1D);
	lcm_dcs_write_seq_static(ctx,0x0A,0x44);
	lcm_dcs_write_seq_static(ctx,0x0B,0xA0);
	lcm_dcs_write_seq_static(ctx,0x0C,0xB0);
	lcm_dcs_write_seq_static(ctx,0x0D,0x80);
	lcm_dcs_write_seq_static(ctx,0x1B,0x70);
	lcm_dcs_write_seq_static(ctx,0x1C,0xF7);
	lcm_dcs_write_seq_static(ctx,0x1D,0xDD);
	lcm_dcs_write_seq_static(ctx,0x1E,0x3A);
	lcm_dcs_write_seq_static(ctx,0x1F,0xE1);
	lcm_dcs_write_seq_static(ctx,0x20,0xF2);
	lcm_dcs_write_seq_static(ctx,0x21,0x4A);
	lcm_dcs_write_seq_static(ctx,0x22,0x2A);
	lcm_dcs_write_seq_static(ctx,0x23,0xA8);
	lcm_dcs_write_seq_static(ctx,0x24,0xDC);
	lcm_dcs_write_seq_static(ctx,0x25,0x41);
	lcm_dcs_write_seq_static(ctx,0x26,0x04);
	lcm_dcs_write_seq_static(ctx,0x27,0x0A);
	lcm_dcs_write_seq_static(ctx,0x28,0x08);
	lcm_dcs_write_seq_static(ctx,0x29,0x00);
	lcm_dcs_write_seq_static(ctx,0x0D,0xC0);
	lcm_dcs_write_seq_static(ctx,0x0E,0xFF);
	lcm_dcs_write_seq_static(ctx,0x0F,0xFF);
	lcm_dcs_write_seq_static(ctx,0x10,0x57);
	lcm_dcs_write_seq_static(ctx,0x11,0x85);
	lcm_dcs_write_seq_static(ctx,0x12,0xCB);
	lcm_dcs_write_seq_static(ctx,0x13,0x2B);
	lcm_dcs_write_seq_static(ctx,0x14,0xA9);
	lcm_dcs_write_seq_static(ctx,0x15,0xA0);
	lcm_dcs_write_seq_static(ctx,0x16,0x72);
	lcm_dcs_write_seq_static(ctx,0x17,0x07);
	lcm_dcs_write_seq_static(ctx,0x18,0x11);
	lcm_dcs_write_seq_static(ctx,0x19,0x28);
	lcm_dcs_write_seq_static(ctx,0x1A,0x2C);
	lcm_dcs_write_seq_static(ctx,0x1B,0x90);
	//mdelay(300);   //prize add by wangfei for resume 20221208
                                         
	lcm_dcs_write_seq_static(ctx,0xFE,0x41);
	lcm_dcs_write_seq_static(ctx,0xD6,0x01);
	lcm_dcs_write_seq_static(ctx,0xFE,0x16);
	lcm_dcs_write_seq_static(ctx,0x80,0x00);
	lcm_dcs_write_seq_static(ctx,0x8C,0x00);
	lcm_dcs_write_seq_static(ctx,0x8D,0x00);
	lcm_dcs_write_seq_static(ctx,0x73,0x00);
	lcm_dcs_write_seq_static(ctx,0x74,0x00);
	lcm_dcs_write_seq_static(ctx,0x8A,0x03); //MSB amout in left
	lcm_dcs_write_seq_static(ctx,0x8B,0xC0); //LSB amout in left
	lcm_dcs_write_seq_static(ctx,0x90,0x00);
	lcm_dcs_write_seq_static(ctx,0x91,0x00);
	lcm_dcs_write_seq_static(ctx,0x75,0x00);
	lcm_dcs_write_seq_static(ctx,0x76,0x00);
	lcm_dcs_write_seq_static(ctx,0x8E,0x03); //MSB amout in right
	lcm_dcs_write_seq_static(ctx,0x8F,0xC0); //LSB amout in right
	lcm_dcs_write_seq_static(ctx,0xFE,0x00);
	lcm_dcs_write_seq_static(ctx,0x77,0x84,0x80,0x04,0x44,0x04,0xC2,0x9D,0x2B,0x05,0x06,0xBE,0x33,0x05,0x4A,0x5A,0xF8,0x05,0x8C,0xEB,0x7B,0x05,0xCE,0x73,0xBD,0x06,0x0F,0x7F,0xFF,0x84,0x00,0x04,0x65,0x04,0x44,0x2D,0xF2,0x04,0x8A,0xDF,0x3B,0x04,0xCE,0x77,0xDF,0x83,0xA0,0x04,0x88,0x03,0xE6,0x46,0xB8,0x04,0x2D,0xF7,0xDF,0x83,0x50,0x04,0x89,0x03,0x97,0x53,0x1C,0x03,0xDF,0x7F,0xFF,0x83,0x20,0x11,0x31,0x03,0x6C,0x6F,0xDF,0x82,0xF0,0x11,0x96,0x03,0x3D,0xFB,0xFF,0x82,0xD0,0x1E,0x5B,0x82,0xA0,0x05,0x55,0x02,0xEE,0xFF,0xFF,0x82,0x80,0x05,0x57,0x02,0xCF,0x7F,0xFF,0x82,0x70,0x22,0xFE,0x82,0x50,0x1E,0xBD,0x82,0x30,0x0E,0x1C,0x82,0x10,0x05,0x79,0x82,0x00,0x12,0x9E,0x81,0xF0,0x2F,0x5F);
	lcm_dcs_write_seq_static(ctx,0x78,0x81,0xD0,0x0A,0x5E,0x81,0xC0,0x1A,0xDF,0x81,0xB0,0x27,0x5F,0x81,0xA0,0x33,0x9F,0x81,0x80,0x06,0x1E,0x81,0x70,0x0A,0x7E,0x81,0x60,0x0A,0x9F,0x81,0x50,0x0A,0x9F,0x81,0x40,0x0A,0x9F,0x81,0x30,0x0A,0x9F,0x81,0x20,0x06,0x7F,0x81,0x10,0x06,0x1E,0x81,0x10,0x37,0xDF,0x81,0x00,0x27,0x9F,0x80,0xF0,0x1B,0x5F,0x80,0xE0,0x0E,0xFF,0x80,0xE0,0x47,0xFF,0x80,0xD0,0x2F,0xBF,0x80,0xC0,0x17,0x5F,0x80,0xB0,0x06,0x9F,0x80,0xB0,0x2F,0xDF,0x80,0xA0,0x13,0x3F,0x80,0xA0,0x47,0xFF,0x80,0x90,0x1F,0x9F,0x80,0x80,0x06,0xBF,0x80,0x80,0x27,0xDF,0x80,0x70,0x06,0xDF,0x80,0x70,0x33,0xDF,0x80,0x60,0x0B,0x1F,0x80,0x60,0x2B,0xDF,0x80,0x50,0x06,0xDF,0x80,0x50,0x23,0xDF);
	lcm_dcs_write_seq_static(ctx,0x78,0x80,0x50,0x4F,0xFF,0x80,0x40,0x13,0x7F,0x80,0x40,0x33,0xFF,0x80,0x40,0x57,0xFF,0x80,0x30,0x13,0x7F,0x80,0x30,0x2B,0xFF,0x80,0x30,0x47,0xFF,0x80,0x20,0x06,0xFF,0x80,0x20,0x13,0x9F,0x80,0x20,0x23,0xDF,0x80,0x20,0x3B,0xFF,0x80,0x20,0x53,0xFF,0x80,0x10,0x07,0x1F,0x80,0x10,0x13,0x9F,0x80,0x10,0x1F,0xBF,0x80,0x10,0x2F,0xFF,0x80,0x10,0x43,0xFF,0x80,0x10,0x57,0xFF,0x80,0x00,0x07,0x1F,0x80,0x00,0x0F,0x5F,0x80,0x00,0x17,0xBF,0x80,0x00,0x27,0xDF,0x80,0x00,0x2F,0xFF,0x80,0x00,0x43,0xFF,0x80,0x00,0x4F,0xFF,0x80,0x00,0x5B,0xFF,0x80,0x00,0xE3,0xFF,0x80,0x01,0x6B,0xFF,0x80,0x02,0x73,0xFF,0x80,0x03,0x77,0xFF,0x80,0x04,0x7B,0xFF,0x80,0x05,0x7F,0xFF);
	lcm_dcs_write_seq_static(ctx,0x78,0x80,0x06,0xFF,0xFF,0x80,0x07,0x7F,0xFF,0x80,0x08,0xFF,0xFF,0x80,0x09,0x7F,0xFF,0x80,0x0A,0x7F,0xFF,0x80,0x0A,0xFF,0xFF,0x80,0x0B,0x7F,0xFF,0x80,0x0B,0xFF,0xFF,0x80,0x0C,0x7F,0xFF,0x80,0x0C,0xFF,0xFF,0x80,0x0D,0x7F,0xFF,0x80,0x0D,0xFF,0xFF,0x80,0x0D,0xFF,0xFF,0x80,0x0E,0x7F,0xFF,0x80,0x0E,0x7F,0xFF,0x80,0x0E,0xFF,0xFF,0x80,0x0E,0xFF,0xFF,0x80,0x0F,0x7F,0xFF,0x80,0x0F,0x7F,0xFF,0xFF,0xF0,0x08,0x9C,0x80,0x0F,0x7F,0xFF,0x80,0x0F,0x7F,0xFF,0x80,0x0E,0xFF,0xFF,0x80,0x0E,0xFF,0xFF,0x80,0x0E,0x7F,0xFF,0x80,0x0E,0x7F,0xFF,0x80,0x0D,0xFF,0xFF,0x80,0x0D,0xFF,0xFF,0x80,0x0D,0x7F,0xFF,0x80,0x0C,0xFF,0xFF,0x80,0x0C,0x7F,0xFF,0x80,0x0B,0xFF,0xFF);
	lcm_dcs_write_seq_static(ctx,0x78,0x80,0x0B,0x7F,0xFF,0x80,0x0A,0xFF,0xFF,0x80,0x09,0xFF,0xFF,0x80,0x09,0x7F,0xFF,0x80,0x08,0x7F,0xFF,0x80,0x07,0x7F,0xFF,0x80,0x06,0x7F,0xFF,0x80,0x04,0xFF,0xFF,0x80,0x04,0x7B,0xFF,0x80,0x03,0x77,0xFF,0x80,0x02,0x6F,0xFF,0x80,0x01,0x6B,0xFF,0x80,0x00,0xE3,0xFF,0x80,0x00,0x57,0xFF,0x80,0x00,0x4B,0xFF,0x80,0x00,0x3F,0xFF,0x80,0x00,0x2F,0xFF,0x80,0x00,0x23,0xDF,0x80,0x00,0x17,0xBF,0x80,0x00,0x0F,0x5F,0x80,0x00,0x07,0x1F,0x80,0x10,0x53,0xFF,0x80,0x10,0x43,0xFF,0x80,0x10,0x2F,0xFF,0x80,0x10,0x1F,0xBF,0x80,0x10,0x13,0x9F,0x80,0x10,0x07,0x1F,0x80,0x20,0x4F,0xFF,0x80,0x20,0x3B,0xFF,0x80,0x20,0x23,0xDF,0x80,0x20,0x13,0x9F,0x80,0x20,0x06,0xFF);
	lcm_dcs_write_seq_static(ctx,0x78,0x80,0x30,0x47,0xFF,0x80,0x30,0x2B,0xFF,0x80,0x30,0x13,0x7F,0x80,0x40,0x57,0xFF,0x80,0x40,0x33,0xFF,0x80,0x40,0x13,0x7F,0x80,0x50,0x4F,0xFF,0x80,0x50,0x23,0xDF,0x80,0x50,0x06,0xDF,0x80,0x60,0x2B,0xDF,0x80,0x60,0x0B,0x1F,0x80,0x70,0x33,0xFF,0x80,0x70,0x0A,0xDF,0x80,0x80,0x27,0xDF,0x80,0x80,0x06,0xBF,0x80,0x90,0x1F,0x9F,0x80,0xA0,0x47,0xFF,0x80,0xA0,0x13,0x3F,0x80,0xB0,0x2F,0xDF,0x80,0xB0,0x06,0x9F,0x80,0xC0,0x17,0x5F,0x80,0xD0,0x2F,0xBF,0x80,0xE0,0x47,0xFF,0x80,0xE0,0x0E,0xFF,0x80,0xF0,0x1B,0x5F,0x81,0x00,0x27,0x9F,0x81,0x10,0x33,0xBF,0x81,0x20,0x43,0xDF,0x81,0x20,0x06,0x7F,0x81,0x30,0x0A,0x9F,0x81,0x40,0x0A,0x9F,0x81,0x50,0x0A,0x9F);
	lcm_dcs_write_seq_static(ctx,0x78,0x81,0x60,0x0A,0x9F,0x81,0x70,0x0A,0x7E,0x81,0x80,0x06,0x1E,0x81,0xA0,0x33,0xBF,0x81,0xB0,0x2B,0x5F,0x81,0xC0,0x1A,0xFF,0x81,0xD0,0x0E,0x5E,0x81,0xF0,0x2F,0x7F,0x82,0x00,0x1A,0xBE,0x82,0x10,0x05,0x7A,0x82,0x30,0x12,0x5D,0x82,0x50,0x1E,0xBE,0x82,0x60,0x05,0x57,0x82,0x80,0x09,0x99,0x02,0xCF,0x7F,0xFF,0x82,0xA0,0x09,0x57,0x02,0xEF,0x7F,0xFF,0x82,0xC0,0x05,0x34,0x03,0x0E,0x7F,0xFF,0x82,0xF0,0x15,0xB6,0x03,0x3D,0xFF,0xFF,0x83,0x20,0x11,0x51,0x03,0x6C,0x73,0xDF,0x83,0x50,0x04,0x89,0x03,0x97,0x57,0x3C,0x03,0xDF,0x7F,0xFF,0x83,0xA0,0x04,0x88,0x03,0xE6,0x46,0xB8,0x04,0x2D,0xF7,0xDF,0x84,0x00,0x04,0x65,0x04,0x44,0x2D,0xF2,0x04,0x8A,0xDF,0x3B);
	lcm_dcs_write_seq_static(ctx,0x78,0x04,0xCE,0x77,0xDF,0x84,0x80,0x04,0x44,0x04,0xC2,0x9D,0x2B,0x05,0x06,0xBE,0x32,0x05,0x4A,0x5A,0xF8,0x05,0x8C,0xEB,0x5B,0x05,0xCE,0x73,0xBD,0x06,0x0F,0x7B,0xFF,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x9B,0xEF,0xFB,0xBD,0x1C,0x2E,0x73,0x7B,0x1C,0x6D,0x67,0x17,0x1C,0xAB,0x52,0x71,0x1C,0xE7,0xB5,0x69,0x1D,0x23,0x94,0x82,0x1D,0x60,0x80,0x00,0x9D,0x0F,0xFB,0xBC,0x1D,0x4D,0xE6,0xF5,0x1D,0x89,0x3D,0x68,0x1D,0xC2,0x8C,0x20,0x9D,0xAF,0xFB,0xBB,0x1D,0xEC,0x56,0x2C,0x1E,0x24,0x10,0x20,0x9E,0x1F,0xFB,0x98,0x1E,0x5A,0x39,0x24);
	lcm_dcs_write_seq_static(ctx,0x78,0x1E,0x90,0x80,0x00,0x9E,0x6F,0xFB,0x78,0x1E,0xA8,0xA4,0x80,0x9E,0xAF,0xFB,0x76,0x1E,0xE6,0x10,0x00,0x9E,0xEF,0xEE,0x47,0x9F,0x0F,0xF6,0xAA,0x1F,0x40,0x80,0x00,0x9F,0x2F,0xFA,0xEA,0x1F,0x60,0x80,0x00,0x9F,0x4F,0xFA,0xE8,0x9F,0x6F,0xF6,0xA7,0x9F,0x8F,0xF2,0x03,0x9F,0xAF,0xE5,0x61,0x9F,0xBF,0xFA,0x84,0x9F,0xDF,0xE9,0x60,0x9F,0xEF,0xFA,0x42,0xA0,0x0F,0xD8,0xC0,0xA0,0x1F,0xE9,0x20,0xA0,0x2F,0xF1,0x80,0xA0,0x3F,0xFA,0x01,0xA0,0x4F,0xFA,0x62,0xA0,0x6F,0xD0,0x40,0xA0,0x7F,0xD0,0x40,0xA0,0x8F,0xD0,0x40,0xA0,0x9F,0xD0,0x40,0xA0,0xAF,0xCC,0x20,0xA0,0xAF,0xFA,0x01,0xA0,0xBF,0xF9,0xA0,0xA0,0xCF,0xF1,0x20,0xA0,0xDF,0xE8,0xC0,0xA0,0xEF,0xDC,0x60);
	lcm_dcs_write_seq_static(ctx,0x78,0xA0,0xFF,0xC4,0x00,0xA0,0xFF,0xF5,0x60,0xA1,0x0F,0xE8,0xA0,0xA1,0x1F,0xD0,0x20,0xA1,0x1F,0xF9,0x60,0xA1,0x2F,0xE4,0x80,0xA1,0x3F,0xC4,0x00,0xA1,0x3F,0xF0,0xE0,0xA1,0x4F,0xD4,0x20,0xA1,0x4F,0xF9,0x20,0xA1,0x5F,0xD8,0x20,0xA1,0x5F,0xF9,0x80,0xA1,0x6F,0xE0,0x40,0xA1,0x6F,0xF9,0x40,0xA1,0x7F,0xD8,0x20,0xA1,0x7F,0xF9,0x00,0xA1,0x8F,0xCC,0x00,0xA1,0x8F,0xEC,0x80,0xA1,0x9F,0xB0,0x00,0xA1,0x9F,0xD4,0x00,0xA1,0x9F,0xEC,0x80,0xA1,0xAF,0xA8,0x00,0xA1,0xAF,0xC4,0x00,0xA1,0xAF,0xDC,0x20,0xA1,0xAF,0xF0,0x80,0xA1,0xAF,0xF9,0x00,0xA1,0xBF,0xB8,0x00,0xA1,0xBF,0xD0,0x00,0xA1,0xBF,0xE0,0x20,0xA1,0xBF,0xF0,0x80,0xA1,0xBF,0xF4,0xE0,0xA1,0xCF,0xAC,0x00);
	lcm_dcs_write_seq_static(ctx,0x78,0xA1,0xCF,0xC0,0x00,0xA1,0xCF,0xD4,0x00,0xA1,0xCF,0xE0,0x20,0xA1,0xCF,0xE8,0x60,0xA1,0xCF,0xF4,0xA0,0xA1,0xCF,0xF9,0x20,0xA1,0xDF,0xAC,0x00,0xA1,0xDF,0xC0,0x00,0xA1,0xDF,0xCC,0x00,0xA1,0xDF,0xD8,0x00,0xA1,0xDF,0xE0,0x21,0xA1,0xDF,0xE8,0x42,0xA1,0xDF,0xF0,0x84,0xA1,0xDF,0xF4,0xC6,0xA1,0xDF,0xF9,0x08,0xA1,0xEF,0xA9,0x4A,0xA1,0xEF,0xB5,0xAD,0xA1,0xEF,0xB9,0xCE,0xA1,0xEF,0xC6,0x31,0xA1,0xEF,0xCA,0x52,0xA1,0xEF,0xD2,0x94,0xA1,0xEF,0xD6,0xB5,0xA1,0xEF,0xDA,0xD6,0xA1,0xEF,0xDE,0xF7,0xA1,0xEF,0xE3,0x18,0xA1,0xEF,0xE7,0x39,0xA1,0xEF,0xEB,0x5A,0xA1,0xEF,0xEF,0x7B,0xA1,0xEF,0xEF,0x7B,0xA1,0xEF,0xF3,0x9C,0xA1,0xEF,0xF3,0x9C,0xA1,0xEF,0xF7,0xBD);
	lcm_dcs_write_seq_static(ctx,0x78,0xA1,0xEF,0xF7,0xBD,0xA1,0xEF,0xFB,0xDE,0xA1,0xEF,0xFB,0xDE,0xFF,0xF0,0x08,0x9C,0xA1,0xEF,0xFB,0xDE,0xA1,0xEF,0xFB,0xDE,0xA1,0xEF,0xF7,0xBD,0xA1,0xEF,0xF7,0xBD,0xA1,0xEF,0xF3,0x9C,0xA1,0xEF,0xF3,0x9C,0xA1,0xEF,0xEF,0x7B,0xA1,0xEF,0xEF,0x7B,0xA1,0xEF,0xEB,0x5A,0xA1,0xEF,0xE7,0x39,0xA1,0xEF,0xE3,0x18,0xA1,0xEF,0xDE,0xF7,0xA1,0xEF,0xDA,0xD6,0xA1,0xEF,0xD6,0xB5,0xA1,0xEF,0xCE,0x73,0xA1,0xEF,0xCA,0x52,0xA1,0xEF,0xC2,0x10,0xA1,0xEF,0xB9,0xCE,0xA1,0xEF,0xB1,0x8C,0xA1,0xEF,0xA5,0x29,0xA1,0xDF,0xF9,0x08,0xA1,0xDF,0xF4,0xC6,0xA1,0xDF,0xEC,0x84,0xA1,0xDF,0xE8,0x42,0xA1,0xDF,0xE0,0x21,0xA1,0xDF,0xD4,0x00,0xA1,0xDF,0xC8,0x00,0xA1,0xDF,0xBC,0x00);
	lcm_dcs_write_seq_static(ctx,0x78,0xA1,0xDF,0xAC,0x00,0xA1,0xCF,0xF9,0x00,0xA1,0xCF,0xF4,0xA0,0xA1,0xCF,0xE8,0x60,0xA1,0xCF,0xE0,0x20,0xA1,0xCF,0xD0,0x00,0xA1,0xCF,0xC0,0x00,0xA1,0xCF,0xAC,0x00,0xA1,0xBF,0xF4,0xE0,0xA1,0xBF,0xF0,0x80,0xA1,0xBF,0xE0,0x20,0xA1,0xBF,0xCC,0x00,0xA1,0xBF,0xB8,0x00,0xA1,0xAF,0xF9,0x00,0xA1,0xAF,0xF0,0x80,0xA1,0xAF,0xDC,0x20,0xA1,0xAF,0xC4,0x00,0xA1,0xAF,0xA8,0x00,0xA1,0x9F,0xEC,0x80,0xA1,0x9F,0xD4,0x00,0xA1,0x9F,0xB0,0x00,0xA1,0x8F,0xEC,0x80,0xA1,0x8F,0xCC,0x00,0xA1,0x7F,0xF9,0x00,0xA1,0x7F,0xD8,0x20,0xA1,0x6F,0xF9,0x40,0xA1,0x6F,0xE0,0x40,0xA1,0x6F,0xB0,0x00,0xA1,0x5F,0xD8,0x40,0xA1,0x4F,0xF9,0x20,0xA1,0x4F,0xD4,0x20,0xA1,0x3F,0xF0,0xE0);
	lcm_dcs_write_seq_static(ctx,0x78,0xA1,0x3F,0xC4,0x00,0xA1,0x2F,0xE4,0x80,0xA1,0x1F,0xF9,0x60,0xA1,0x1F,0xD0,0x20,0xA1,0x0F,0xE8,0xA0,0xA0,0xFF,0xF5,0x60,0xA0,0xFF,0xC4,0x00,0xA0,0xEF,0xDC,0x60,0xA0,0xDF,0xE8,0xC0,0xA0,0xCF,0xF1,0x20,0xA0,0xBF,0xF5,0x80,0xA0,0xAF,0xFA,0x00,0xA0,0xAF,0xCC,0x20,0xA0,0x9F,0xD0,0x40,0xA0,0x8F,0xD0,0x40,0xA0,0x7F,0xD0,0x40,0xA0,0x6F,0xD0,0x40,0xA0,0x4F,0xFA,0x62,0xA0,0x3F,0xFA,0x01,0xA0,0x2F,0xF5,0x80,0xA0,0x1F,0xE9,0x40,0xA0,0x0F,0xDC,0xC0,0x9F,0xEF,0xFA,0x43,0x9F,0xDF,0xED,0x60,0x9F,0xBF,0xFA,0xA6,0x9F,0xAF,0xE9,0x61,0x9F,0x8F,0xF6,0x44,0x9F,0x6F,0xFA,0xA7,0x9F,0x5F,0xDD,0x41,0x9F,0x2F,0xFB,0x2C,0x1F,0x61,0x00,0x00,0x9F,0x0F,0xFA,0xEA);
	lcm_dcs_write_seq_static(ctx,0x78,0x1F,0x41,0x00,0x00,0x9E,0xEF,0xF2,0x89,0x1F,0x20,0x80,0x00,0x9E,0xBF,0xEE,0xCD,0x1E,0xF2,0x80,0x00,0x9E,0x6F,0xFB,0x98,0x1E,0xA8,0xA8,0x80,0x9E,0x1F,0xFB,0x99,0x1E,0x5A,0xB9,0x24,0x1E,0x90,0x80,0x00,0x9D,0xAF,0xFB,0xBB,0x1D,0xEC,0x56,0x2C,0x1E,0x24,0x10,0x20,0x9D,0x0F,0xFB,0xBC,0x1D,0x4D,0xE6,0xF5,0x1D,0x89,0x3D,0x68,0x1D,0xC2,0x8C,0x20,0x9B,0xDF,0xFB,0xDD,0x1C,0x1E,0xF3,0x9B,0x1C,0x5D,0x6B,0x38,0x1C,0x9B,0xDA,0x92,0x1C,0xD8,0xBD,0xAB,0x1D,0x14,0x9C,0xA4,0x1D,0x51,0x04,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00);

	lcm_dcs_write_seq_static(ctx,0xFE,0x41);
	lcm_dcs_write_seq_static(ctx,0xD6,0x00);

	lcm_dcs_write_seq_static(ctx,0xFE,0x16);
	lcm_dcs_write_seq_static(ctx,0x8A,0x83);


	lcm_dcs_write_seq_static(ctx,0xFE,0x71);
	lcm_dcs_write_seq_static(ctx,0x82,0x01);
	lcm_dcs_write_seq_static(ctx,0xC6,0x00);
	lcm_dcs_write_seq_static(ctx,0xC7,0x21);
	lcm_dcs_write_seq_static(ctx,0xC8,0x1D);
	lcm_dcs_write_seq_static(ctx,0xC9,0x39);
	lcm_dcs_write_seq_static(ctx,0xCA,0xB5);
	lcm_dcs_write_seq_static(ctx,0xCB,0x02);
	lcm_dcs_write_seq_static(ctx,0xCC,0x01);
	lcm_dcs_write_seq_static(ctx,0xCD,0x56);
	lcm_dcs_write_seq_static(ctx,0xCE,0x16);
	lcm_dcs_write_seq_static(ctx,0xCF,0x7E);
	lcm_dcs_write_seq_static(ctx,0xD0,0xC8);
	lcm_dcs_write_seq_static(ctx,0xD1,0x82);
	lcm_dcs_write_seq_static(ctx,0xD2,0x00);
	lcm_dcs_write_seq_static(ctx,0xD3,0x00);
	lcm_dcs_write_seq_static(ctx,0xD4,0x00);
	lcm_dcs_write_seq_static(ctx,0xD5,0x00);
	lcm_dcs_write_seq_static(ctx,0xD6,0x00);
	lcm_dcs_write_seq_static(ctx,0xDE,0x08);
	lcm_dcs_write_seq_static(ctx,0xDF,0x00);
	lcm_dcs_write_seq_static(ctx,0xE0,0x02);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00);
	lcm_dcs_write_seq_static(ctx,0xE2,0x00);

	lcm_dcs_write_seq_static(ctx,0xFE,0x16);
	lcm_dcs_write_seq_static(ctx,0x41,0xFF);
	lcm_dcs_write_seq_static(ctx,0x42,0xFF);
	lcm_dcs_write_seq_static(ctx,0x43,0xFF);

	//spr
	lcm_dcs_write_seq_static(ctx,0xFE,0x90);
	lcm_dcs_write_seq_static(ctx,0x59,0x02);
	lcm_dcs_write_seq_static(ctx,0x52,0x10);
	lcm_dcs_write_seq_static(ctx,0x4C,0x02);
	lcm_dcs_write_seq_static(ctx,0x5A,0x02);
	lcm_dcs_write_seq_static(ctx,0x53,0x10);
	lcm_dcs_write_seq_static(ctx,0x4D,0x02);
	lcm_dcs_write_seq_static(ctx,0xFE,0x90);
	lcm_dcs_write_seq_static(ctx,0x70,0x0C);
	lcm_dcs_write_seq_static(ctx,0x55,0x04);
	lcm_dcs_write_seq_static(ctx,0x45,0x00);
	lcm_dcs_write_seq_static(ctx,0x6C,0x00);
	lcm_dcs_write_seq_static(ctx,0x4F,0x10);
	lcm_dcs_write_seq_static(ctx,0x41,0x00);
	lcm_dcs_write_seq_static(ctx,0x68,0x0C);
	lcm_dcs_write_seq_static(ctx,0x49,0x04);
	lcm_dcs_write_seq_static(ctx,0x3B,0x00);
	lcm_dcs_write_seq_static(ctx,0x72,0x0C);
	lcm_dcs_write_seq_static(ctx,0x58,0x04);
	lcm_dcs_write_seq_static(ctx,0x47,0x00);
	lcm_dcs_write_seq_static(ctx,0x6E,0x00);
	lcm_dcs_write_seq_static(ctx,0x51,0x10);
	lcm_dcs_write_seq_static(ctx,0x43,0x00);
	lcm_dcs_write_seq_static(ctx,0x6A,0x0C);
	lcm_dcs_write_seq_static(ctx,0x4B,0x04);
	lcm_dcs_write_seq_static(ctx,0x3F,0x00);
	lcm_dcs_write_seq_static(ctx,0x64,0x0C);
	lcm_dcs_write_seq_static(ctx,0x27,0x04);
	lcm_dcs_write_seq_static(ctx,0x37,0x00);
	lcm_dcs_write_seq_static(ctx,0x60,0x00);
	lcm_dcs_write_seq_static(ctx,0x21,0x10);
	lcm_dcs_write_seq_static(ctx,0x33,0x00);
	lcm_dcs_write_seq_static(ctx,0x5C,0x0C);
	lcm_dcs_write_seq_static(ctx,0x1B,0x04);
	lcm_dcs_write_seq_static(ctx,0x2F,0x00);
	lcm_dcs_write_seq_static(ctx,0x66,0x0C);
	lcm_dcs_write_seq_static(ctx,0x29,0x04);
	lcm_dcs_write_seq_static(ctx,0x39,0x00);
	lcm_dcs_write_seq_static(ctx,0x62,0x00);
	lcm_dcs_write_seq_static(ctx,0x23,0x10);
	lcm_dcs_write_seq_static(ctx,0x35,0x00);
	lcm_dcs_write_seq_static(ctx,0x5E,0x0C);
	lcm_dcs_write_seq_static(ctx,0x1D,0x04);
	lcm_dcs_write_seq_static(ctx,0x31,0x00);

	lcm_dcs_write_seq_static(ctx,0xFE,0xA1);
	lcm_dcs_write_seq_static(ctx,0xCD,0x00);
	lcm_dcs_write_seq_static(ctx,0xCE,0x00);
	lcm_dcs_write_seq_static(ctx,0xCA,0x80);
	lcm_dcs_write_seq_static(ctx,0xFE,0x40);
	lcm_dcs_write_seq_static(ctx,0xBD,0x00);

//1080*2400   VESA  DSC1.1  3x   ( H1V60 10bpc_10bpp )
lcm_dcs_write_seq_static(ctx,0xFE,0xD2); // switch to D2 page  

lcm_dcs_write_seq_static(ctx,0x50,0x11); // pps000{0x7:3]{0x3:0) =(cfg) DSC_VERSION_MAJOR{0x3:0], DSC_VERSION_MINOR{0x3:0)
lcm_dcs_write_seq_static(ctx,0x51,0xab); // pps003{0x7:4]{0x3:0)=(cfg) bpc{0x3:0], linebuf_depth{0x3:0) = {bits_per_component{0x3:0], LINE_BUFFER_BPC{0x3:0)
lcm_dcs_write_seq_static(ctx,0x52,0x30); // pps004{0x5]{0x4]{0x3]{0x2]{0x1:0] =(cfg) BP, not_useYuvInput(enc_convert_RGB2YCoCg), simple422, VBR, bpp{0x9:8) = {BLOCK_PRED_ENABLE, ~USE_YUV_INPUT, SIMPLE_422, VBR_ENABLE, bits_per_pixel{0x9:8)
lcm_dcs_write_seq_static(ctx,0x53,0x09); // pps006{0x7:0)=(cfg) pic_height{0x15:8)
lcm_dcs_write_seq_static(ctx,0x54,0x60); // pps007{0x7:0)=(cfg) pic_height{0x7:0)
lcm_dcs_write_seq_static(ctx,0x55,0x04); // pps008{0x7:0)=(cfg) pic_width{0x15:8)
lcm_dcs_write_seq_static(ctx,0x56,0x38); // pps009{0x7:0)=(cfg) pic_width{0x7:0] }
lcm_dcs_write_seq_static(ctx,0x58,0x00); // pps010{0x7:0)=(cfg) slice_height{0x15:8) = SLICE_HEIGHT{0x15:8)
lcm_dcs_write_seq_static(ctx,0x59,0x28); // pps011{0x7:0)=(cfg) slice_height{0x7:0)  = SLICE_HEIGHT{0x7:0) 
lcm_dcs_write_seq_static(ctx,0x5a,0x04); // pps012{0x7:0)=(cfg) slice_width{0x15:8)  = SLICE_WIDTH{0x15:8)
lcm_dcs_write_seq_static(ctx,0x5b,0x38); // pps013{0x7:0)=(cfg) slice_width{0x7:0)   = SLICE_WIDTH{0x7:0) 
lcm_dcs_write_seq_static(ctx,0x5c,0x01); // pps016{0x1:0)=(cfg) initial_xmit_delay{0x9:8) = {INITIAL_DELAY{0x9:8)
lcm_dcs_write_seq_static(ctx,0x5d,0x9a); // pps017{0x7:0)=(cfg) initial_xmit_delay{0x7:0) = {INITIAL_DELAY{0x7:0)
lcm_dcs_write_seq_static(ctx,0x5e,0x19); // pps021{0x5:0)=(dyn) initial_scale_value{0x5:0)
lcm_dcs_write_seq_static(ctx,0x5f,0x04); // pps022{0x7:0)=(dyn) scale_increment_interval{0x15:8)
lcm_dcs_write_seq_static(ctx,0x60,0xce); // pps023{0x7:0)=(dyn) scale_increment_interval{0x7:0)
lcm_dcs_write_seq_static(ctx,0x61,0x00); // pps024{0x3:0)=(dyn) scale_decrement_interval{0x11:8)
lcm_dcs_write_seq_static(ctx,0x62,0x15); // pps025{0x7:0)=(dyn) scale_decrement_interval{0x7:0)
lcm_dcs_write_seq_static(ctx,0x63,0x0c); // pps027{0x4:0)=(dyn) first_line_bpg_offset{0x4:0) = {first_line_bpg_ofs{0x4:0)
lcm_dcs_write_seq_static(ctx,0x64,0x02); // pps028{0x7:0)=(dyn) nfl_bpg_offset{0x15:8)
lcm_dcs_write_seq_static(ctx,0x65,0x77); // pps029{0x7:0)=(dyn) nfl_bpg_offset{0x7:0)
lcm_dcs_write_seq_static(ctx,0x66,0x01); // pps030{0x7:0)=(dyn) slice_bpg_offset{0x15:8)
lcm_dcs_write_seq_static(ctx,0x67,0x8f); // pps031{0x7:0)=(dyn) slice_bpg_offset{0x7:0)
lcm_dcs_write_seq_static(ctx,0x68,0x16); // pps032{0x7:0)=(cfg) initial_offset{0x15:8) = {INITIAL_FULLNESS_OFFSET{0x15:8)
lcm_dcs_write_seq_static(ctx,0x69,0x00); // pps033{0x7:0)=(cfg) initial_offset{0x7:0)  = {INITIAL_FULLNESS_OFFSET {0x7:0)
lcm_dcs_write_seq_static(ctx,0x6a,0x10); // pps034{0x7:0)=(dyn) final_offset{0x15:8)
lcm_dcs_write_seq_static(ctx,0x6b,0xec); // pps035{0x7:0)=(dyn) final_offset{0x7:0)
lcm_dcs_write_seq_static(ctx,0x6c,0x07); // pps036{0x4:0)=(cfg) flatness_min_qp{0x4:0) = {FLATNESS_MIN_QP{0x4:0)
lcm_dcs_write_seq_static(ctx,0x6d,0x10); // pps037{0x4:0)=(cfg) flatness_max_qp{0x4:0) = {FLATNESS_MAX_QP{0x4:0)
lcm_dcs_write_seq_static(ctx,0x6e,0x20); // pps038{0x7:0)=(cfg) rc_model_size{0x15:8) = {RC_MODEL_SIZE{0x15:8)
lcm_dcs_write_seq_static(ctx,0x6f,0x00); // pps039{0x7:0)=(cfg) rc_model_size{0x7:0)  = {RC_MODEL_SIZE{0x7:0)
lcm_dcs_write_seq_static(ctx,0x70,0x06); // pps040{0x3:0)=(cfg) rc_edge_factor{0x3:0) = {RC_EDGE_FACTOR{0x3:0)
lcm_dcs_write_seq_static(ctx,0x71,0x0f); // pps041{0x4:0)=(cfg) rc_quant_incr_limit0{0x4:0) = {RC_QUANT_INCR_LIMIT0{0x4:0)
lcm_dcs_write_seq_static(ctx,0x72,0x0f); // pps042{0x4:0)=(cfg) rc_quant_incr_limit1{0x4:0) = {RC_QUANT_INCR_LIMIT1{0x4:0)
lcm_dcs_write_seq_static(ctx,0x73,0x33); // pps043{0x7:4]{0x3:0)=(cfg) rc_tgt_offset_hi{0x3:0], rc_tgt_offset_lo{0x3:0) = {RC_TGT_OFFSET_HI{0x3:0], RC_TGT_OFFSET_LO{0x3:0)
lcm_dcs_write_seq_static(ctx,0x74,0x0e); // pps044{0x7:0]<<6=(cfg) rc_buf_thresh{0x0] {0x13:0) = RC_BUF_THRESH{0x0] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x75,0x1c); // pps045{0x7:0]<<6=(cfg) rc_buf_thresh{0x1] {0x13:0) = RC_BUF_THRESH{0x1] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x76,0x2a); // pps046{0x7:0]<<6=(cfg) rc_buf_thresh{0x2] {0x13:0) = RC_BUF_THRESH{0x2] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x77,0x38); // pps047{0x7:0]<<6=(cfg) rc_buf_thresh{0x3] {0x13:0) = RC_BUF_THRESH{0x3] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x78,0x46); // pps048{0x7:0]<<6=(cfg) rc_buf_thresh{0x4] {0x13:0) = RC_BUF_THRESH{0x4] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x79,0x54); // pps049{0x7:0]<<6=(cfg) rc_buf_thresh{0x5] {0x13:0) = RC_BUF_THRESH{0x5] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x7a,0x62); // pps050{0x7:0]<<6=(cfg) rc_buf_thresh{0x6] {0x13:0) = RC_BUF_THRESH{0x6] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x7b,0x69); // pps051{0x7:0]<<6=(cfg) rc_buf_thresh{0x7] {0x13:0) = RC_BUF_THRESH{0x7] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x7c,0x70); // pps052{0x7:0]<<6=(cfg) rc_buf_thresh{0x8] {0x13:0) = RC_BUF_THRESH{0x8] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x7d,0x77); // pps053{0x7:0]<<6=(cfg) rc_buf_thresh{0x9] {0x13:0) = RC_BUF_THRESH{0x9] {0x13:0]
lcm_dcs_write_seq_static(ctx,0x7e,0x79); // pps054{0x7:0]<<6=(cfg) rc_buf_thresh{0x10]{0x13:0) = RC_BUF_THRESH{0x10]{0x13:0]
lcm_dcs_write_seq_static(ctx,0x7f,0x7b); // pps055{0x7:0]<<6=(cfg) rc_buf_thresh{0x11]{0x13:0) = RC_BUF_THRESH{0x11]{0x13:0]
lcm_dcs_write_seq_static(ctx,0x80,0x7d); // pps056{0x7:0]<<6=(cfg) rc_buf_thresh{0x12]{0x13:0) = RC_BUF_THRESH{0x12]{0x13:0]
lcm_dcs_write_seq_static(ctx,0x81,0x7e); // pps057{0x7:0]<<6=(cfg) rc_buf_thresh{0x13]{0x13:0) = RC_BUF_THRESH{0x13]{0x13:0]
lcm_dcs_write_seq_static(ctx,0x82,0x01); // pps058{0x7:3]{0x2:0)=(cfg) range_min_qp_0{0x4:0] , range_max_qp_0{0x4:2]      } = {RC_MINQP{0x0]{0x4:0], RC_MAXQP{0x0]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x83,0xc2); // pps059{0x7:6]{0x5:0)=(cfg) range_max_qp_0{0x1:0] , range_bpg_offset_0{0x5:0]  } = {RC_MAXQP{0x0]{0x1:0], RC_OFFSET{0x0]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x84,0x22); // pps060{0x7:3]{0x2:0)=(cfg) range_min_qp_1{0x4:0] , range_max_qp_1{0x4:2]      } = {RC_MINQP{0x1]{0x4:0], RC_MAXQP{0x1]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x85,0x00); // pps061{0x7:6]{0x5:0)=(cfg) range_max_qp_1{0x1:0] , range_bpg_offset_1{0x5:0]  } = {RC_MAXQP{0x1]{0x1:0], RC_OFFSET{0x1]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x86,0x2a); // pps062{0x7:3]{0x2:0)=(cfg) range_min_qp_2{0x4:0] , range_max_qp_2{0x4:2]      } = {RC_MINQP{0x2]{0x4:0], RC_MAXQP{0x2]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x87,0x40); // pps063{0x7:6]{0x5:0)=(cfg) range_max_qp_2{0x1:0] , range_bpg_offset_2{0x5:0]  } = {RC_MAXQP{0x2]{0x1:0], RC_OFFSET{0x2]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x88,0x32); // pps064{0x7:3]{0x2:0)=(cfg) range_min_qp_3{0x4:0] , range_max_qp_3{0x4:2]      } = {RC_MINQP{0x3]{0x4:0], RC_MAXQP{0x3]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x89,0xbe); // pps065{0x7:6]{0x5:0)=(cfg) range_max_qp_3{0x1:0] , range_bpg_offset_3{0x5:0]  } = {RC_MAXQP{0x3]{0x1:0], RC_OFFSET{0x3]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x8a,0x3a); // pps066{0x7:3]{0x2:0)=(cfg) range_min_qp_4{0x4:0] , range_max_qp_4{0x4:2]      } = {RC_MINQP{0x4]{0x4:0], RC_MAXQP{0x4]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x8b,0xfc); // pps067{0x7:6]{0x5:0)=(cfg) range_max_qp_4{0x1:0] , range_bpg_offset_4{0x5:0]  } = {RC_MAXQP{0x4]{0x1:0], RC_OFFSET{0x4]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x8c,0x3a); // pps068{0x7:3]{0x2:0)=(cfg) range_min_qp_5{0x4:0] , range_max_qp_5{0x4:2]      } = {RC_MINQP{0x5]{0x4:0], RC_MAXQP{0x5]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x8d,0xfa); // pps069{0x7:6]{0x5:0)=(cfg) range_max_qp_5{0x1:0] , range_bpg_offset_5{0x5:0]  } = {RC_MAXQP{0x5]{0x1:0], RC_OFFSET{0x5]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x8e,0x3a); // pps070{0x7:3]{0x2:0)=(cfg) range_min_qp_6{0x4:0] , range_max_qp_6{0x4:2]      } = {RC_MINQP{0x6]{0x4:0], RC_MAXQP{0x6]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x8f,0xf8); // pps071{0x7:6]{0x5:0)=(cfg) range_max_qp_6{0x1:0] , range_bpg_offset_6{0x5:0]  } = {RC_MAXQP{0x6]{0x1:0], RC_OFFSET{0x6]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x90,0x3b); // pps072{0x7:3]{0x2:0)=(cfg) range_min_qp_7{0x4:0] , range_max_qp_7{0x4:2]      } = {RC_MINQP{0x7]{0x4:0], RC_MAXQP{0x7]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x91,0x38); // pps073{0x7:6]{0x5:0)=(cfg) range_max_qp_7{0x1:0] , range_bpg_offset_7{0x5:0]  } = {RC_MAXQP{0x7]{0x1:0], RC_OFFSET{0x7]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x92,0x3b); // pps074{0x7:3]{0x2:0)=(cfg) range_min_qp_8{0x4:0] , range_max_qp_8{0x4:2]      } = {RC_MINQP{0x8]{0x4:0], RC_MAXQP{0x8]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x93,0x78); // pps075{0x7:6]{0x5:0)=(cfg) range_max_qp_8{0x1:0] , range_bpg_offset_8{0x5:0]  } = {RC_MAXQP{0x8]{0x1:0], RC_OFFSET{0x8]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x94,0x3b); // pps076{0x7:3]{0x2:0)=(cfg) range_min_qp_9{0x4:0] , range_max_qp_9{0x4:2]      } = {RC_MINQP{0x9]{0x4:0], RC_MAXQP{0x9]{0x4:2]    }
lcm_dcs_write_seq_static(ctx,0x95,0x76); // pps077{0x7:6]{0x5:0)=(cfg) range_max_qp_9{0x1:0] , range_bpg_offset_9{0x5:0]  } = {RC_MAXQP{0x9]{0x1:0], RC_OFFSET{0x9]{0x5:0]   }
lcm_dcs_write_seq_static(ctx,0x96,0x4b); // pps078{0x7:3]{0x2:0)=(cfg) range_min_qp_10{0x4:0], range_max_qp_10{0x4:2]     } = {RC_MINQP{0x10]{0x4:0], RC_MAXQP{0x10]{0x4:2]  }
lcm_dcs_write_seq_static(ctx,0x97,0xb6); // pps079{0x7:6]{0x5:0)=(cfg) range_max_qp_10{0x1:0], range_bpg_offset_10{0x5:0] } = {RC_MAXQP{0x10]{0x1:0], RC_OFFSET{0x10]{0x5:0] }
lcm_dcs_write_seq_static(ctx,0x98,0x4b); // pps080{0x7:3]{0x2:0)=(cfg) range_min_qp_11{0x4:0], range_max_qp_11{0x4:2]     } = {RC_MINQP{0x11]{0x4:0], RC_MAXQP{0x11]{0x4:2]  }
lcm_dcs_write_seq_static(ctx,0x99,0xf6); // pps081{0x7:6]{0x5:0)=(cfg) range_max_qp_11{0x1:0], range_bpg_offset_11{0x5:0] } = {RC_MAXQP{0x11]{0x1:0], RC_OFFSET{0x11]{0x5:0] }
lcm_dcs_write_seq_static(ctx,0x9a,0x4c); // pps082{0x7:3]{0x2:0)=(cfg) range_min_qp_12{0x4:0], range_max_qp_12{0x4:2]     } = {RC_MINQP{0x12]{0x4:0], RC_MAXQP{0x12]{0x4:2]  }
lcm_dcs_write_seq_static(ctx,0x9b,0x34); // pps083{0x7:6]{0x5:0)=(cfg) range_max_qp_12{0x1:0], range_bpg_offset_12{0x5:0] } = {RC_MAXQP{0x12]{0x1:0], RC_OFFSET{0x12]{0x5:0] }
lcm_dcs_write_seq_static(ctx,0x9c,0x5c); // pps084{0x7:3]{0x2:0)=(cfg) range_min_qp_13{0x4:0], range_max_qp_13{0x4:2]     } = {RC_MINQP{0x13]{0x4:0], RC_MAXQP{0x13]{0x4:2]  }
lcm_dcs_write_seq_static(ctx,0x9d,0x74); // pps085{0x7:6]{0x5:0)=(cfg) range_max_qp_13{0x1:0], range_bpg_offset_13{0x5:0] } = {RC_MAXQP{0x13]{0x1:0], RC_OFFSET{0x13]{0x5:0] }
lcm_dcs_write_seq_static(ctx,0x9e,0x8c); // pps086{0x7:3]{0x2:0)=(cfg) range_min_qp_14{0x4:0], range_max_qp_14{0x4:2]     } = {RC_MINQP{0x14]{0x4:0], RC_MAXQP{0x14]{0x4:2]  }
lcm_dcs_write_seq_static(ctx,0x9f,0xf4); // pps087{0x7:6]{0x5:0)=(cfg) range_max_qp_14{0x1:0], range_bpg_offset_14{0x5:0] } = {RC_MAXQP{0x14]{0x1:0], RC_OFFSET{0x14]{0x5:0] }
lcm_dcs_write_seq_static(ctx,0xa2,0x05); // pps014{0x7:0)=(dyn) chunk_size{0x15:8)
lcm_dcs_write_seq_static(ctx,0xa3,0x46); // pps015{0x7:0)=(dyn) chunk_size{0x7:0)
lcm_dcs_write_seq_static(ctx,0xa4,0x00); // pps088{0x1]{0x0]  =(cfg) native_420, native_422} = {NATIVE_420, NATIVE_422}
lcm_dcs_write_seq_static(ctx,0xa5,0x00); // pps089{0x4:0)=(dyn) second_line_bpg_offset{0x4:0)
lcm_dcs_write_seq_static(ctx,0xa6,0x00); // pps090{0x7:0)=(dyn) nsl_bpg_offset{0x15:8)
lcm_dcs_write_seq_static(ctx,0xa7,0x00); // pps091{0x7:0)=(dyn) nsl_bpg_offset{0x7:0)
lcm_dcs_write_seq_static(ctx,0xa9,0x00); // pps092{0x7:0)=(dyn) second_line_offset_adj{0x15:8)
lcm_dcs_write_seq_static(ctx,0xaa,0x00); // pps093{0x7:0)=(dyn) second_line_offset_adj{0x7:0)
lcm_dcs_write_seq_static(ctx,0xa0,0xa0); // pps005{0x7:0)=(cfg) bpp{0x7:0) = {bits_per_pixel{0x7:0)

lcm_dcs_write_seq_static(ctx,0x4F,0x08); // 0x4FD2=0x08, use ENG PPS

lcm_dcs_write_seq_static(ctx,0xFE,0xD6); //1080
lcm_dcs_write_seq_static(ctx,0x06,0x11); 
lcm_dcs_write_seq_static(ctx,0x03,0x01); 
lcm_dcs_write_seq_static(ctx,0x04,0xB4); 
lcm_dcs_write_seq_static(ctx,0x05,0x68); 

	lcm_dcs_write_seq_static(ctx,0xFE,0x00);
	lcm_dcs_write_seq_static(ctx,0xFA,0x01);
	lcm_dcs_write_seq_static(ctx,0xC2,0x08);
	lcm_dcs_write_seq_static(ctx,0x35,0x00);
	/*PRIZE:Added by lvyuanchuan,X9-678,20221230*/
	lcm_pannel_reconfig_blk(ctx);
	lcm_dcs_write_seq_static(ctx,0x11,0x00);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx,0x29,0x00);
	//prize add by majiangtao for frequency Select 20230103 start
	mdelay(10);
#if 0    //prize add by yinhanhan for hbm, 20230311 start
	if (g_vrefresh != -1)
	{
		if (g_vrefresh == MODE_0_FPS) /*60HZ*/
		{
			printk("[panel] %s mode_switch MODE_0_FPS 60HZ\n",__func__);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
			lcm_dcs_write_seq_static(ctx, 0xBD, 0x00);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
		}
		else if (g_vrefresh == MODE_1_FPS) /*90HZ*/
		{
			printk("[panel] %s mode_switch MODE_1_FPS 90HZ\n",__func__);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
			lcm_dcs_write_seq_static(ctx, 0xBD, 0x06);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
		}
		else if (g_vrefresh == MODE_2_FPS) /*120HZ*/
		{
			printk("[panel] %s mode_switch MODE_2_FPS 120HZ\n",__func__);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
			lcm_dcs_write_seq_static(ctx, 0xBD, 0x05);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
		}
		else
		{
			printk("[panel] %s invalid g_vrefresh:%d\n",__func__, g_vrefresh);
		}
	}
#endif   //prize add by yinhanhan for hbm, 20230311  end
	//prize add by majiangtao for frequency Select 20230103 end
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;
	pr_err("gezi----exit------%s-----%d\n",__func__,__LINE__);
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);

	ctx->error = 0;
	ctx->prepared = false;
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_disable();
	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);
#else
	pr_err("gezi------exit----%s-----%d\n",__func__,__LINE__);
	//reset
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	udelay(3000);
	// 138   --  2.8
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	udelay(3000);

	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);

	//137  -  1.2
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(2000);


	//prize add by wangfei for ldo 1.8 20210709 start
	ctx->pm_enable_gpio = devm_gpiod_get_index(ctx->dev,
		"pm-enable", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->pm_enable_gpio)) {
		dev_err(ctx->dev, "%s: cannot get pm_enable_gpio %ld\n",
			__func__, PTR_ERR(ctx->pm_enable_gpio));
		return PTR_ERR(ctx->pm_enable_gpio);
	}
	gpiod_set_value(ctx->pm_enable_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->pm_enable_gpio);
	//prize add by wangfei for ldo 1.8 20210709 end

#endif
	ctx->hbm_en = false;
	/*przie update hbm_stat X9LAVA-953 20230329*/
	ctx->hbm_stat = false;
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);

	if (ctx->prepared)
		return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
#else

	//prize add by wangfei for ldo 1.8 20210709 start
	ctx->pm_enable_gpio = devm_gpiod_get_index(ctx->dev,
		"pm-enable", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->pm_enable_gpio)) {
		dev_err(ctx->dev, "%s: cannot get pm_enable_gpio %ld\n",
			__func__, PTR_ERR(ctx->pm_enable_gpio));
		return PTR_ERR(ctx->pm_enable_gpio);
	}
	gpiod_set_value(ctx->pm_enable_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->pm_enable_gpio);
	//prize add by wangfei for ldo 1.8 20210709 end
	udelay(3000);


	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(3000);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
#endif


	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	//mtk_panel_tch_rst(panel);
	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}
	ctx->enabled = true;

	return 0;
}

#define VAC (2400)
#define HAC (1080)
static u32 fake_heigh = 2400;
static u32 fake_width = 1080;
static bool need_fake_resolution;

static const struct drm_display_mode switch_mode_120 = {
	.clock = 327060,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_2_HFP,
	.hsync_end = FRAME_WIDTH + MODE_2_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_2_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,
	.vrefresh = MODE_2_FPS,
};


static const struct drm_display_mode switch_mode_90 = {
	.clock = 266090,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_1_HFP,
	.hsync_end = FRAME_WIDTH + MODE_1_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_1_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
	.vrefresh = MODE_1_FPS,
};

static const struct drm_display_mode default_mode = {
	.clock = 227090,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
	.vrefresh = MODE_0_FPS,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	//struct gpio_desc *id2_gpio = NULL;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	//unsigned char id[3] = {0x0, 0x80, 0x0};
	ssize_t ret;
	pr_err("panel----exit------%s-----%d\n",__func__,__LINE__);

	// prize baibo for lcm ata test begin
#if 1
	//lcm_dcs_write_seq_static(ctx,0xFE,0xC2);
	ret = mipi_dsi_dcs_read(dsi, 0x04, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	printk("panel_ata_check-ATA read 0x04 data %x %x %x\n", data[0], data[1], data[2]);

	if (data[1] == 0x80) {
		return 1;
	}
	
	return 0;
#endif
#if 0
	id2_gpio = devm_gpiod_get(ctx->dev, "id2", GPIOD_IN);
	if (IS_ERR(id2_gpio)) {
		dev_err(ctx->dev, "%s: cannot get id2_gpio %ld\n",
			__func__, PTR_ERR(id2_gpio));
		return 0;
	}

	if (gpiod_get_value(id2_gpio)) {
		pr_err("%s %d id2 value 1\n",__func__,__LINE__);
		ret = 0;
	}else{
		pr_err("%s %d id2 value 0\n",__func__,__LINE__);
		ret = 1;
	}
	devm_gpiod_put(ctx->dev, id2_gpio);

	return ret;
	// prize baibo for lcm ata test end
#endif
}

extern void prize_common_node_show_register(char* name,bool(*hbm_set)(void));
bool get_hbmstate(void)
{
	printk("%s g_ctx->hbm_stat:%d",__func__, g_ctx->hbm_stat);
	return g_ctx->hbm_stat;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51,0x07,0xFF};
	char hbm_tb[] = {0x51,0x0F,0xFF};
	unsigned int level_normal = 125;
	unsigned int reg_level = 125;

	if(level){
		reg_level = Gamma_to_level[level] + BLK_LEVEL_OFFSET;
		bl_level = level;
	}
	else
		reg_level = 0;
	g_current_level = level;
	bl_tb0[1] = (reg_level>>8)&0xf;
	bl_tb0[2] = (reg_level)&0xff;
	pr_err("level{ %d - %d },bl_tb0[1] = %d,bl_tb0[2] = %d\n",level,reg_level,bl_tb0[1],bl_tb0[2]);
	if (!cb)
		return -1;
	if(g_ctx->hbm_stat == false || level == 0)
		cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	 return 0;
}


//prize add by gongtaitao for sensorhub get backlight 20221028 start
unsigned short led_level_disp_get(char *name)
{
    int trans_level = 0;
	trans_level = Gamma_to_level[g_current_level];
	pr_err("[%s]: name: %s, level : %d",__func__, name, trans_level);
	return trans_level;
}
EXPORT_SYMBOL(led_level_disp_get);
//prize add by gongtaitao for sensorhub get backlight 20221028 end



static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
			      dcs_write_gce cb, void *handle, bool en)
{
	unsigned int level_hbm = 255;
	unsigned int level_normal = 125;
	char normal_tb0[] = {0x51, 0x07,0xFF};
	char hbm_tb[] = {0x51,0x0F,0xFF};
	struct lcm *ctx = panel_to_lcm(panel);

	if (!cb)
		return -1;

	//if (ctx->hbm_en == en)
	//	goto done;

	if (en)
	{
		g_ctx->hbm_stat = true;
		cb(dsi, handle, hbm_tb, ARRAY_SIZE(hbm_tb));
	}
	else
	{
		printk("[panel] %s : set normal = %d\n",__func__,bl_level);
		level_normal = Gamma_to_level[bl_level] + BLK_LEVEL_OFFSET;
		normal_tb0[1] = (level_normal>>8)&0xff;
		normal_tb0[2] = (level_normal)&0xff;
		g_ctx->hbm_stat = false;
		cb(dsi, handle, normal_tb0, ARRAY_SIZE(normal_tb0));
	}

	ctx->hbm_en = en;
	ctx->hbm_wait = true;

 done:
	return 0;
}

static void panel_hbm_get_state(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);

	*state = ctx->hbm_en;
}

static void panel_hbm_get_wait_state(struct drm_panel *panel, bool *wait)
{
	struct lcm *ctx = panel_to_lcm(panel);

	*wait = ctx->hbm_wait;
}

static bool panel_hbm_set_wait_state(struct drm_panel *panel, bool wait)
{
	struct lcm *ctx = panel_to_lcm(panel);
	bool old = ctx->hbm_wait;

	ctx->hbm_wait = wait;
	return old;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_params ext_params_120 = {
	// .vfp_low_power = 743,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
		.para_list[1] = 0xdc,
	},
	.hbm_en_time = 0,
	.hbm_dis_time = 1,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
			.enable = 1,
			.bdg_dsc_enable = 0,
			.ver                   =  DSC_VER,
			.slice_mode            =  DSC_SLICE_MODE,
			.rgb_swap              =  DSC_RGB_SWAP,
			.dsc_cfg               =  DSC_DSC_CFG,
			.rct_on                =  DSC_RCT_ON,
			.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable             =  DSC_BP_ENABLE,
			.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
			.pic_height            =  FRAME_HEIGHT,
			.pic_width             =  FRAME_WIDTH,
			.slice_height          =  DSC_SLICE_HEIGHT,
			.slice_width           =  DSC_SLICE_WIDTH,
			.chunk_size            =  DSC_CHUNK_SIZE,
			.xmit_delay            =  DSC_XMIT_DELAY,
			.dec_delay             =  DSC_DEC_DELAY,
			.scale_value           =  DSC_SCALE_VALUE,
			.increment_interval    =  DSC_INCREMENT_INTERVAL,
			.decrement_interval    =  DSC_DECREMENT_INTERVAL,
			.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
			.initial_offset        =  DSC_INITIAL_OFFSET,
			.final_offset          =  DSC_FINAL_OFFSET,
			.flatness_minqp        =  DSC_FLATNESS_MINQP,
			.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
			.rc_model_size         =  DSC_RC_MODEL_SIZE,
			.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		},
//	.wait_sof_before_dec_vfp = 1,

	.dyn_fps = {
		.switch_en = 1,
	},
	.data_rate = MODE_2_DATA_RATE,
	.bdg_ssc_disable = 1,
	.ssc_disable = 1,
	.dyn = {
		.switch_en = 1,
		.data_rate = MODE_2_DATA_RATE,
	},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
};

static struct mtk_panel_params ext_params_90 = {
	// .vfp_low_power = 743,
	.hbm_en_time = 0,
	.hbm_dis_time = 1,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
		.para_list[1] = 0xdc,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
			.enable = 1,
			.bdg_dsc_enable = 0,
			.ver                   =  DSC_VER,
			.slice_mode            =  DSC_SLICE_MODE,
			.rgb_swap              =  DSC_RGB_SWAP,
			.dsc_cfg               =  DSC_DSC_CFG,
			.rct_on                =  DSC_RCT_ON,
			.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable             =  DSC_BP_ENABLE,
			.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
			.pic_height            =  FRAME_HEIGHT,
			.pic_width             =  FRAME_WIDTH,
			.slice_height          =  DSC_SLICE_HEIGHT,
			.slice_width           =  DSC_SLICE_WIDTH,
			.chunk_size            =  DSC_CHUNK_SIZE,
			.xmit_delay            =  DSC_XMIT_DELAY,
			.dec_delay             =  DSC_DEC_DELAY,
			.scale_value           =  DSC_SCALE_VALUE,
			.increment_interval    =  DSC_INCREMENT_INTERVAL,
			.decrement_interval    =  DSC_DECREMENT_INTERVAL,
			.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
			.initial_offset        =  DSC_INITIAL_OFFSET,
			.final_offset          =  DSC_FINAL_OFFSET,
			.flatness_minqp        =  DSC_FLATNESS_MINQP,
			.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
			.rc_model_size         =  DSC_RC_MODEL_SIZE,
			.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		},
//	.wait_sof_before_dec_vfp = 1,

	.dyn_fps = {
		.switch_en = 1,
	},
	.data_rate = MODE_2_DATA_RATE,
	.bdg_ssc_disable = 1,
	.ssc_disable = 1,
	.dyn = {
		.switch_en = 1,
		.data_rate = MODE_1_DATA_RATE,
	},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
};


static struct mtk_panel_params ext_params = {
	// .pll_clk = 373,
	// .vfp_low_power = 743,
	.hbm_en_time = 0,
	.hbm_dis_time = 1,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
		.para_list[1] = 0xdc,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
			.enable = 1,
			.bdg_dsc_enable = 0,
			.ver                   =  DSC_VER,
			.slice_mode            =  DSC_SLICE_MODE,
			.rgb_swap              =  DSC_RGB_SWAP,
			.dsc_cfg               =  DSC_DSC_CFG,
			.rct_on                =  DSC_RCT_ON,
			.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
			.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
			.bp_enable             =  DSC_BP_ENABLE,
			.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
			.pic_height            =  FRAME_HEIGHT,
			.pic_width             =  FRAME_WIDTH,
			.slice_height          =  DSC_SLICE_HEIGHT,
			.slice_width           =  DSC_SLICE_WIDTH,
			.chunk_size            =  DSC_CHUNK_SIZE,
			.xmit_delay            =  DSC_XMIT_DELAY,
			.dec_delay             =  DSC_DEC_DELAY,
			.scale_value           =  DSC_SCALE_VALUE,
			.increment_interval    =  DSC_INCREMENT_INTERVAL,
			.decrement_interval    =  DSC_DECREMENT_INTERVAL,
			.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
			.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
			.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
			.initial_offset        =  DSC_INITIAL_OFFSET,
			.final_offset          =  DSC_FINAL_OFFSET,
			.flatness_minqp        =  DSC_FLATNESS_MINQP,
			.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
			.rc_model_size         =  DSC_RC_MODEL_SIZE,
			.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
			.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
			.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
			.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
			.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		},
//	.wait_sof_before_dec_vfp = 1,
	.data_rate = MODE_2_DATA_RATE,
	.bdg_ssc_disable = 1,
	.ssc_disable = 1,
	.dyn_fps = {
		.switch_en = 1,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = MODE_0_DATA_RATE,
	},
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
};


struct drm_display_mode *get_mode_by_id(struct drm_panel *panel,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &panel->connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, mode);
	printk("[panel] %s,vrefresh = %d\n",__func__,m->vrefresh);
	if (m->vrefresh == MODE_0_FPS)
		ext->params = &ext_params;
	else if (m->vrefresh == MODE_1_FPS)
		ext->params = &ext_params_90;
	else if (m->vrefresh == MODE_2_FPS)
		ext->params = &ext_params_120;
	else
		ret = 1;

	return ret;
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		printk("[panel] %s\n",__func__);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
			lcm_dcs_write_seq_static(ctx, 0xBD, 0x05);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	}
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		printk("[panel] %s\n",__func__);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
			lcm_dcs_write_seq_static(ctx, 0xBD, 0x06);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		printk("[panel] %s\n",__func__);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
			lcm_dcs_write_seq_static(ctx, 0xBD, 0x00);
			lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	}
}

static int mode_switch(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, dst_mode);
	printk("[panel] %s,cur_mode = %d,dst_mode = %d\n",__func__,cur_mode,dst_mode);
	if (cur_mode == dst_mode)
		return ret;

	if (m->vrefresh == MODE_0_FPS) { /*switch to 60 */
		mode_switch_to_60(panel, stage);
	} else if (m->vrefresh == MODE_1_FPS) { /*switch to 90 */
		mode_switch_to_90(panel, stage);
	} else if (m->vrefresh == MODE_2_FPS) { /*switch to 120 */
		mode_switch_to_120(panel, stage);
	} else
		ret = 1;
	//prize add by majiangtao for frequency Select 20230103 start
	g_vrefresh = m->vrefresh;
	//prize add by majiangtao for frequency Select 20230103 end
	return ret;
}
extern int run_to_goodix_ts_suspend(void);
/*PRIZE:added by lvyuanchuan,x9-750,20230110 start*/
static int panel_doze_enable_start(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("panel %s\n", __func__);
	panel_ext_reset(panel, 0);
	usleep_range(10 * 1000, 15 * 1000);
	panel_ext_reset(panel, 1);

	lcm_dcs_write_seq_static(ctx, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);
	return 0;
}


static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("panel %s\n", __func__);
	/*PageA0*/
	lcm_dcs_write_seq_static(ctx, 0xFE, 0xA0);
	/*AOD推力减小*/
	lcm_dcs_write_seq_static(ctx, 0x25, 0xC0);
	lcm_dcs_write_seq_static(ctx, 0x3A, 0x60);
	lcm_dcs_write_seq_static(ctx, 0x22, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x37, 0x8F);
	/*Page40*/
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x40);
	/*Inter power on*/
	lcm_dcs_write_seq_static(ctx, 0xAC, 0x84);
	/*Page26*/
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x26);
	/*Fast discharge off*/
	lcm_dcs_write_seq_static(ctx, 0x22, 0x1A);
	/*Page00*/
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	/*Idle mode on*/
	lcm_dcs_write_seq_static(ctx, 0x39);
	/*Enter AOD 30nit*/
	lcm_dcs_write_seq_static(ctx, 0x51, 0x09, 0x56);
	run_to_goodix_ts_suspend();
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("panel %s\n", __func__);

	/*Page00*/
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	/*Idle mode off*/
	lcm_dcs_write_seq_static(ctx, 0x38);
	/*Page26*/
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x26);
	/*Fast discharge ON*/
	lcm_dcs_write_seq_static(ctx, 0x22, 0x19);
	/*PageA0*/
	lcm_dcs_write_seq_static(ctx, 0xFE, 0xA0);
	/*返回Normal推力*/
	lcm_dcs_write_seq_static(ctx, 0x25, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x3A, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x22, 0x44);
	lcm_dcs_write_seq_static(ctx, 0x37, 0x88);
	//prize add by durunshen for lcd 20230320, X9LAVA-462 start
	/*Page00*/
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	//prize add by durunshen for lcd 20230320, X9LAVA-462 end
	return 0;
}

static int panel_set_aod_light_mode(void *dsi,
	dcs_write_gce cb, void *handle, unsigned int mode)
{
	int i = 0;

	pr_info("panel %s\n", __func__);

	if (mode >= 1) {
		/*Enter AOD 50nit*/
		pr_info("panel %s Enter AOD 50nit\n", __func__);
		lcm_dcs_write_seq_static(g_ctx, 0x51, 0x09, 0x56);
	} else {
		/*Enter AOD 30nit*/
		pr_info("panel %s Enter AOD 30nit\n", __func__);
		lcm_dcs_write_seq_static(g_ctx, 0x51, 0x00, 0x05);
	}
	pr_info("%s : %d !\n", __func__, mode);

	return 0;
}
/*PRIZE:added by lvyuanchuan,x9-750,20230110 end*/

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.hbm_set_cmdq = panel_hbm_set_cmdq,
	.hbm_get_state = panel_hbm_get_state,
	.hbm_get_wait_state = panel_hbm_get_wait_state,
	.hbm_set_wait_state = panel_hbm_set_wait_state,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	/*aod mode*/
	.doze_enable = panel_doze_enable,
	//.doze_enable_start = panel_doze_enable_start,
	.doze_disable = panel_doze_disable,
	.set_aod_light_mode = panel_set_aod_light_mode,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};
/*
static void change_drm_disp_mode_params(struct drm_display_mode *mode)
{
	if (fake_heigh > 0 && fake_heigh < VAC) {
		mode->vdisplay = fake_heigh;
		mode->vsync_start = fake_heigh + VFP;
		mode->vsync_end = fake_heigh + VFP + VSA;
		mode->vtotal = fake_heigh + VFP + VSA + VBP;
	}
	if (fake_width > 0 && fake_width < HAC) {
		mode->hdisplay = fake_width;
		mode->hsync_start = fake_width + HFP;
		mode->hsync_end = fake_width + HFP + HSA;
		mode->htotal = fake_width + HFP + HSA + HBP;
	}
}
*/
static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;

	//if (need_fake_resolution)
	//	change_drm_disp_mode_params(&default_mode);
	
	mode = drm_mode_duplicate(panel->drm, &switch_mode_120);
	if (!mode) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_120.hdisplay, switch_mode_120.vdisplay,
			switch_mode_120.vrefresh);
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(panel->connector, mode);
	printk("[panel] %s,333\n",__func__);
	
	
	mode_1 = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode_1) {
		dev_err(panel->drm->dev, "failed to add mode_1 %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode_1);
	printk("[panel] %s,111\n",__func__);

	mode_2 = drm_mode_duplicate(panel->drm, &switch_mode_90);
	if (!mode_2) {
		dev_err(panel->drm->dev, "failed to add mode_2 %ux%ux@%u\n",
			switch_mode_90.hdisplay, switch_mode_90.vdisplay,
			switch_mode_90.vrefresh);
		return -ENOMEM;
	}
	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(panel->connector, mode_2);
	printk("[panel] %s,222\n",__func__);

	

	panel->connector->display_info.width_mm = 64;
	panel->connector->display_info.height_mm = 129;

	return 3;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static void check_is_need_fake_resolution(struct device *dev)
{
	unsigned int ret = 0;

	ret = of_property_read_u32(dev->of_node, "fake_heigh", &fake_heigh);
	if (ret)
		need_fake_resolution = false;
	ret = of_property_read_u32(dev->of_node, "fake_width", &fake_width);
	if (ret)
		need_fake_resolution = false;
	if (fake_heigh > 0 && fake_heigh < VAC)
		need_fake_resolution = true;
	if (fake_width > 0 && fake_width < HAC)
		need_fake_resolution = true;
	pr_err("%s------need_fake_resolution = %d------%d\n", __func__,need_fake_resolution,__LINE__);
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	pr_err("gezi ---------%d-----\n",__LINE__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_err("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_err("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_err("gezi ---- %s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	// dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
	// 		 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
	// 		 | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_CLOCK_NON_CONTINUOUS;  //prize add by yinhanhan for hbm 20230311
	// dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST;
	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	pr_err("gezi ---------%d-----\n",__LINE__);

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(dev, "%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}

	pr_err("gezi ---------%d-----\n",__LINE__);

	devm_gpiod_put(dev, ctx->bias_neg);

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	pr_err("gezi ---------%d-----\n",__LINE__);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	//mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	check_is_need_fake_resolution(dev);
	pr_err("%s------------%d\n", __func__,__LINE__);

	//add by wangfei
	// lcm_panel_init(ctx);
	g_ctx = ctx;
	ctx->hbm_en = false;
	g_ctx->hbm_stat = false;
	prize_common_node_show_register("HBMSTATE", &get_hbmstate);

#if defined(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"rm692e5.cmd");
    strcpy(current_lcm_info.vendor,"Raydium");
    sprintf(current_lcm_info.id,"0x%02x",0x81);
    strcpy(current_lcm_info.more,"1080*2400");
#endif
	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "Raydium,rm692e5,cmd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-rm692e5-cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Yi-Lun Wang <Yi-Lun.Wang@mediatek.com>");
MODULE_DESCRIPTION("rm692e5 CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");

