// SPDX-License-Identifier: GPL-2.0-only
/*
 * MIPI-DSI based s6e3fc3_6a AMOLED LCD panel driver.
 *
 * Copyright (c) 2021 Google, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <drm/drm_vblank.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <video/mipi_display.h>

#include "samsung/panel/panel-samsung-drv.h"

static const unsigned char PPS_SETTING[] = {
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
	0x04, 0x38, 0x00, 0x30, 0x02, 0x1C, 0x02, 0x1C,
	0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x04, 0xA6,
	0x00, 0x07, 0x00, 0x0C, 0x02, 0x0B, 0x02, 0x1F,
	0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
	0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#define S6E3FC3_6A_WRCTRLD_DIMMING_BIT    0x08
#define S6E3FC3_6A_WRCTRLD_BCTRL_BIT      0x20
#define S6E3FC3_6A_WRCTRLD_HBM_BIT        0xC0
#define S6E3FC3_6A_WRCTRLD_LOCAL_HBM_BIT  0x10

static const u8 display_off[] = { 0x28 };
static const u8 display_on[] = { 0x29 };
static const u8 test_key_on_f0[] = { 0xF0, 0x5A, 0x5A };
static const u8 test_key_off_f0[] = { 0xF0, 0xA5, 0xA5 };
static const u8 test_key_on_f1[] = { 0xF1, 0x5A, 0x5A };
static const u8 test_key_off_f1[] = { 0xF1, 0xA5, 0xA5 };
static const u8 FQ_update[] = { 0xF7, 0x07 };

static const struct exynos_dsi_cmd s6e3fc3_6a_off_cmds[] = {
	EXYNOS_DSI_CMD(display_off, 0),
	EXYNOS_DSI_CMD_SEQ_DELAY(120, 0x10), /* sleep in */
};
static DEFINE_EXYNOS_CMD_SET(s6e3fc3_6a_off);

static const struct exynos_dsi_cmd s6e3fc3_6a_lp_cmds[] = {
	EXYNOS_DSI_CMD(display_off, 0),
};
static DEFINE_EXYNOS_CMD_SET(s6e3fc3_6a_lp);

static const struct exynos_dsi_cmd s6e3fc3_6a_lp_off_cmds[] = {
	EXYNOS_DSI_CMD(display_off, 0)
};

static const struct exynos_dsi_cmd s6e3fc3_6a_lp_low_cmds[] = {
	EXYNOS_DSI_CMD_SEQ_DELAY(17, 0x53, 0x25), /* AOD 10 nit */
	EXYNOS_DSI_CMD(display_on, 0)
};

static const struct exynos_dsi_cmd s6e3fc3_6a_lp_high_cmds[] = {
	EXYNOS_DSI_CMD_SEQ_DELAY(17, 0x53, 0x24), /* AOD 50 nit */
	EXYNOS_DSI_CMD(display_on, 0)
};

static const struct exynos_binned_lp s6e3fc3_6a_binned_lp[] = {
	BINNED_LP_MODE("off", 0, s6e3fc3_6a_lp_off_cmds),
	/* rising time = delay = 0, falling time = delay + width = 0 + 73 */
	BINNED_LP_MODE_TIMING("low", 80, s6e3fc3_6a_lp_low_cmds, 0, 0 + 73),
	BINNED_LP_MODE_TIMING("high", 2047, s6e3fc3_6a_lp_high_cmds, 0, 0 + 73)
};

static const struct exynos_dsi_cmd s6e3fc3_6a_init_cmds[] = {
	EXYNOS_DSI_CMD_SEQ_DELAY(120, 0x11), /* sleep out */
	EXYNOS_DSI_CMD_SEQ(0x35, 0x00), /* TE on */
	EXYNOS_DSI_CMD_SEQ(0x2A, 0x00, 0x00, 0x04, 0x37), /* CASET */
	EXYNOS_DSI_CMD_SEQ(0x2B, 0x00, 0x00, 0x09, 0x5F), /* PASET */

	EXYNOS_DSI_CMD0(test_key_on_f0),

	/* FQ CON setting */
	EXYNOS_DSI_CMD_SEQ(0xB0, 0x27, 0xF2),
	EXYNOS_DSI_CMD_SEQ(0xF2, 0x80),
	EXYNOS_DSI_CMD0(FQ_update),

	/* IRC setting */
	EXYNOS_DSI_CMD_SEQ(0xB0, 0x03, 0x8F),
	EXYNOS_DSI_CMD_SEQ(0x8F, 0x25),

	/* Local HBM circle location setting */
	EXYNOS_DSI_CMD0(test_key_on_f1),
	EXYNOS_DSI_CMD_SEQ(0xB0, 0x28, 0xF2),
	EXYNOS_DSI_CMD_SEQ(0xF2, 0xCC),
	EXYNOS_DSI_CMD_SEQ(0xB0, 0x01, 0x34, 0x68),
	EXYNOS_DSI_CMD_SEQ(0x68, 0x21, 0xC6, 0xB3),
	EXYNOS_DSI_CMD_SEQ(0xB0, 0x00, 0x28, 0xF2),
	EXYNOS_DSI_CMD_SEQ(0xF2, 0xC4),
	EXYNOS_DSI_CMD0(test_key_off_f1),

	EXYNOS_DSI_CMD0(test_key_off_f0)
};
static DEFINE_EXYNOS_CMD_SET(s6e3fc3_6a_init);

static void s6e3fc3_6a_get_te2_setting(struct exynos_panel_te2_timing *timing,
				       u8 *setting)
{
	u8 delay_low_byte, delay_high_byte;
	u8 width_low_byte, width_high_byte;
	u32 rising, falling;

	if (!timing || !setting)
		return;

	rising = timing->rising_edge;
	falling = timing->falling_edge;

	delay_low_byte = rising & 0xFF;
	delay_high_byte = (rising >> 8) & 0xF;
	width_low_byte = (falling - rising) & 0xFF;
	width_high_byte = ((falling - rising) >> 8) & 0xF;

	setting[0] = (delay_high_byte >> 4) | (delay_low_byte >> 4);
	setting[1] = ((delay_low_byte & 0xF) << 4) | (width_high_byte);
	setting[2] = width_low_byte;
}

/* update TE2 when system boots, resumes, OFF->AOD, or sysfs write */
static void s6e3fc3_6a_update_te2(struct exynos_panel *ctx)
{
	struct exynos_panel_te2_timing timing;
	u8 setting[4] = {0xCB, 0x00, 0x00, 0x42}; // normal 60Hz
	u8 lp_setting[4] = {0xCB, 0x00, 0x00, 0x49}; // lp low/high

	if (!ctx)
		return;

	/* normal 60Hz mode */
	timing.rising_edge = ctx->te2.mode_data[0].timing.rising_edge;
	timing.falling_edge = ctx->te2.mode_data[0].timing.falling_edge;
	s6e3fc3_6a_get_te2_setting(&timing, &setting[1]);
	dev_dbg(ctx->dev, "TE2 updated normal 60Hz: 0xcb 0x%x 0x%x 0x%x\n",
		setting[1], setting[2], setting[3]);

	/* LP low/high mode */
	timing.rising_edge = ctx->te2.mode_data[1].timing.rising_edge;
	timing.falling_edge = ctx->te2.mode_data[1].timing.falling_edge;
	s6e3fc3_6a_get_te2_setting(&timing, &lp_setting[1]);
	dev_dbg(ctx->dev, "TE2 updated LP low/high: 0xcb 0x%x 0x%x 0x%x\n",
		lp_setting[1], lp_setting[2], lp_setting[3]);

	EXYNOS_DCS_WRITE_TABLE(ctx, test_key_on_f0);

	if (ctx->panel_rev == PANEL_REV_PROTO1) {
		EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x1C, 0xCB); /* global para  */
		EXYNOS_DCS_WRITE_SEQ(ctx, 0xCB, 0x14); /* TE2 on */
		EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x30, 0xCB); /* global para  */
		EXYNOS_DCS_WRITE_SEQ(ctx, 0xCB, 0x14); /* TE2 on */
		EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x39, 0xCB); /* global para  */
		EXYNOS_DCS_WRITE_SEQ(ctx, 0xCB, 0x6F); /* TE2 on */
	}

	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0xAF, 0xCB); /* global para  */
	EXYNOS_DCS_WRITE_TABLE(ctx, setting); /* normal delay and width */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x28, 0xF2); /* global para */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF2, 0xCC); /* global para 10bit*/
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x01, 0xAF, 0xCB); /* global para */
	EXYNOS_DCS_WRITE_TABLE(ctx, lp_setting); /* HLPM delay and width */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x00, 0x28, 0xF2); /* global para */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF2, 0xC4); /* global para 8bit */
	EXYNOS_DCS_WRITE_TABLE(ctx, FQ_update); /* update */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x26, 0xF2); /* global para */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF2, 0x03, 0x94); /* TE2 on */
	EXYNOS_DCS_WRITE_TABLE(ctx, test_key_off_f0);
}

static void s6e3fc3_6a_update_te2_stub(struct exynos_panel *ctx)
{
	/* create a stub function in order to support TE2 int and sysfs read/write */
}

static int s6e3fc3_6a_configure_te2_edges(struct exynos_panel *ctx,
					  u32 *timings, bool lp_mode)
{
	int ret = exynos_panel_configure_te2_edges(ctx, timings, lp_mode);

	if (ret)
		dev_err(ctx->dev, "failed to configure TE2 edges\n");
	else
		s6e3fc3_6a_update_te2(ctx);

	return ret;
}

static void s6e3fc3_6a_update_wrctrld(struct exynos_panel *ctx)
{
	u8 val = S6E3FC3_6A_WRCTRLD_BCTRL_BIT;

	if (IS_HBM_ON(ctx->hbm_mode))
		val |= S6E3FC3_6A_WRCTRLD_HBM_BIT;

	if (ctx->hbm.local_hbm.enabled)
		val |= S6E3FC3_6A_WRCTRLD_LOCAL_HBM_BIT;

	if (ctx->dimming_on)
		val |= S6E3FC3_6A_WRCTRLD_DIMMING_BIT;

	dev_dbg(ctx->dev,
		"%s(wrctrld:0x%x, hbm: %s, dimming: %s, local_hbm: %s)\n",
		__func__, val, IS_HBM_ON(ctx->hbm_mode) ? "on" : "off",
		ctx->dimming_on ? "on" : "off",
		ctx->hbm.local_hbm.enabled ? "on" : "off");

	EXYNOS_DCS_WRITE_SEQ(ctx, MIPI_DCS_WRITE_CONTROL_DISPLAY, val);

}

static void s6e3fc3_6a_set_nolp_mode(struct exynos_panel *ctx,
				  const struct exynos_panel_mode *pmode)
{
	u32 delay_us = mult_frac(1000, 1020, 60);	/* 60 Hz */

	if (!ctx->enabled)
		return;

	EXYNOS_DCS_WRITE_TABLE(ctx, display_off);
	/* backlight control and dimming */
	s6e3fc3_6a_update_wrctrld(ctx);
	usleep_range(delay_us, delay_us + 10);
	EXYNOS_DCS_WRITE_TABLE(ctx, display_on);

	dev_info(ctx->dev, "exit LP mode\n");
}

#define S6E3FC3_LOCAL_HBM_GAMMA_CMD_SIZE 6
static void s6e3fc3_6a_update_lhbm_gamma(struct exynos_panel *ctx)
{
	/* ratio provided by HW for update the LHBM gamma.
	 * ratio must be a integer due to kernel didn't support floating.
	 * ratio original value R: 1.076923077, G: 1.061566485, B: 1.065269461.
	 * ratio cannot exceed u32 max 4294967296.
	 * R gamma hex from last 16bit from gamma_cmd[1] combine with gamma_cmd[3]
	 * G gamma hex from first 16bit from gamma_cmd[2] combine with gamma_cmd[4]
	 * B gamma hex from last 16bit from gamma_cmd[2] combine with gamma_cmd[5]
	 */
	u8 *gamma_cmd = ctx->hbm.local_hbm.gamma_cmd;
	const u32 rgb_ratio[3] = {1076923077, 1061566485, 1065269461};
	const u8 rgb_index[3][2] = {{1, 3}, {2, 4}, {2, 5}};
	u8 new_gamma_cmd[S6E3FC3_LOCAL_HBM_GAMMA_CMD_SIZE] = {0};
	u64 tmp;
	int i;
	u16 mask, shift;
	dev_info(ctx->dev, "%s: gamma_cmd(%02x %02x %02x %02x %02x)\n", __func__,
		gamma_cmd[1], gamma_cmd[2], gamma_cmd[3], gamma_cmd[4], gamma_cmd[5]);
	for (i = 0; i < ARRAY_SIZE(rgb_ratio); i++) {
		if (i % 2) {
			mask = 0xf0;
			shift = 4;
		} else {
			mask = 0x0f;
			shift = 0;
		}
		tmp = ((gamma_cmd[rgb_index[i][0]] & mask) >> shift) << 8 | gamma_cmd[rgb_index[i][1]];
		dev_dbg(ctx->dev, "%s: lhbm_gamma[%d] = %llu\n", __func__, i, tmp);

		/* Round off and revert to original gamma value */
		tmp = (tmp * rgb_ratio[i] + 500000000)/1000000000;
		dev_dbg(ctx->dev, "%s: new lhbm_gamma[%d] = %llu\n", __func__, i, tmp);
		new_gamma_cmd[rgb_index[i][0]] |= ((tmp & 0xff00) >> 8) << shift;
		new_gamma_cmd[rgb_index[i][1]] |= tmp & 0xff;
	}
	memcpy(&gamma_cmd[1], &new_gamma_cmd[1], S6E3FC3_LOCAL_HBM_GAMMA_CMD_SIZE - 1);
	dev_info(ctx->dev, "%s: new_gamma_cmd(%02x %02x %02x %02x %02x)\n", __func__,
		gamma_cmd[1], gamma_cmd[2], gamma_cmd[3], gamma_cmd[4], gamma_cmd[5]);
}

static int s6e3fc3_6a_lhbm_gamma_read(struct exynos_panel *ctx)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	u8 *gamma_cmd = ctx->hbm.local_hbm.gamma_cmd;
	int ret;

	EXYNOS_DCS_WRITE_TABLE(ctx, test_key_on_f0);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x28, 0xF2); /* global para*/
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF2, 0xCC); /* 10 bit */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x00, 0x22, 0xD8); /* global para */
	ret = mipi_dsi_dcs_read(dsi, 0xD8, gamma_cmd + 1, S6E3FC3_LOCAL_HBM_GAMMA_CMD_SIZE - 1);
	if (ret == (S6E3FC3_LOCAL_HBM_GAMMA_CMD_SIZE - 1)) {
		gamma_cmd[0] = 0x65;
		ctx->hbm.local_hbm.gamma_para_ready = true;
		ret = 0;
	} else {
		dev_err(ctx->dev, "fail to read LHBM gamma\n");
		ret = -EIO;
	}
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x00, 0x28, 0xF2); /* global para*/
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF2, 0xC4); /* 8 bit */
	EXYNOS_DCS_WRITE_TABLE(ctx, test_key_off_f0);
	s6e3fc3_6a_update_lhbm_gamma(ctx);
	return ret;
}

static void s6e3fc3_6a_lhbm_gamma_write(struct exynos_panel *ctx)
{
	EXYNOS_DCS_WRITE_TABLE(ctx, test_key_on_f0);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x28, 0xF2); /* global para*/
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF2, 0xCC); /* 10 bit */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x03, 0xC8, 0x65); /* global para */
	exynos_dcs_write(ctx, ctx->hbm.local_hbm.gamma_cmd,
			 S6E3FC3_LOCAL_HBM_GAMMA_CMD_SIZE); /* write gamma */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x00, 0x28, 0xF2); /* global para*/
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF2, 0xC4); /* 8 bit */
	EXYNOS_DCS_WRITE_TABLE(ctx, test_key_off_f0);
}

static int s6e3fc3_6a_enable(struct drm_panel *panel)
{
	struct exynos_panel *ctx = container_of(panel, struct exynos_panel, panel);
	const struct exynos_panel_mode *pmode = ctx->current_mode;

	if (!pmode) {
		dev_err(ctx->dev, "no current mode set\n");
		return -EINVAL;
	}

	dev_dbg(ctx->dev, "%s\n", __func__);

	exynos_panel_reset(ctx);

	exynos_panel_send_cmd_set(ctx, &s6e3fc3_6a_init_cmd_set);

	if (ctx->panel_rev >= PANEL_REV_PROTO1_1)
		if (ctx->hbm.local_hbm.gamma_para_ready)
			s6e3fc3_6a_lhbm_gamma_write(ctx);

	/* DSC related configuration */
	exynos_dcs_compression_mode(ctx, 0x1); /* DSC_DEC_ON */
	EXYNOS_PPS_LONG_WRITE(ctx); /* PPS_SETTING */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xC2, 0x14); /* PPS_MIC_OFF */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x9D, 0x01); /* PPS_DSC_EN */

	s6e3fc3_6a_update_wrctrld(ctx); /* dimming and HBM */

	ctx->enabled = true;

	if (pmode->exynos_mode.is_lp_mode)
		exynos_panel_set_lp_mode(ctx, pmode);
	else
		EXYNOS_DCS_WRITE_SEQ(ctx, 0x29); /* display on */

	s6e3fc3_6a_update_te2(ctx);

	return 0;
}

static void s6e3fc3_6a_set_hbm_mode(struct exynos_panel *exynos_panel,
				 enum exynos_hbm_mode mode)
{
	const bool hbm_update =
		(IS_HBM_ON(exynos_panel->hbm_mode) != IS_HBM_ON(mode));
	const bool irc_update =
		(IS_HBM_ON_IRC_OFF(exynos_panel->hbm_mode) != IS_HBM_ON_IRC_OFF(mode));

	exynos_panel->hbm_mode = mode;

	if (hbm_update) {
		s6e3fc3_6a_update_wrctrld(exynos_panel);
	}

	if (irc_update) {
		EXYNOS_DCS_WRITE_SEQ(exynos_panel, 0xF0, 0x5A, 0x5A);
		EXYNOS_DCS_WRITE_SEQ(exynos_panel, 0xB0, 0x03, 0x8F);
		EXYNOS_DCS_WRITE_SEQ(exynos_panel, 0x8F, IS_HBM_ON_IRC_OFF(mode) ? 0x05 : 0x25);
		EXYNOS_DCS_WRITE_SEQ(exynos_panel, 0xF0, 0xA5, 0xA5);
	}
	dev_info(exynos_panel->dev, "hbm_on=%d hbm_ircoff=%d\n", IS_HBM_ON(exynos_panel->hbm_mode),
		 IS_HBM_ON_IRC_OFF(exynos_panel->hbm_mode));
}

static void s6e3fc3_6a_set_dimming_on(struct exynos_panel *exynos_panel,
				 bool dimming_on)
{
	const struct exynos_panel_mode *pmode = exynos_panel->current_mode;

	exynos_panel->dimming_on = dimming_on;
	if (pmode->exynos_mode.is_lp_mode) {
		dev_info(exynos_panel->dev,"in lp mode, skip to update");
		return;
	}

	s6e3fc3_6a_update_wrctrld(exynos_panel);
}

static void s6e3fc3_6a_set_local_hbm_mode(struct exynos_panel *exynos_panel,
				 bool local_hbm_en)
{
	if (exynos_panel->hbm.local_hbm.enabled == local_hbm_en)
		return;

	exynos_panel->hbm.local_hbm.enabled = local_hbm_en;
	s6e3fc3_6a_update_wrctrld(exynos_panel);
}

static bool s6e3fc3_6a_is_mode_seamless(const struct exynos_panel *ctx,
					const struct exynos_panel_mode *pmode)
{
	/* seamless mode switch is possible if only changing refresh rate */
	return drm_mode_equal_no_clocks(&ctx->current_mode->mode, &pmode->mode);
}

static void s6e3fc3_6a_panel_init(struct exynos_panel *ctx)
{
	struct dentry *csroot = ctx->debugfs_cmdset_entry;

	exynos_panel_debugfs_create_cmdset(ctx, csroot,
					   &s6e3fc3_6a_init_cmd_set, "init");

	if (ctx->panel_rev >= PANEL_REV_PROTO1_1)
		if (!s6e3fc3_6a_lhbm_gamma_read(ctx))
			s6e3fc3_6a_lhbm_gamma_write(ctx);

	if (ctx->enabled)
		s6e3fc3_6a_update_te2(ctx);
}

static void s6e3fc3_6a_get_panel_rev(struct exynos_panel *ctx, u32 id)
{
	/* extract command 0xDB */
	u8 build_code = (id & 0xFF00) >> 8;
	u8 rev = build_code >> 2;

	switch (rev) {
	case 4:
		ctx->panel_rev = PANEL_REV_PROTO1;
		break;
	case 5:
		ctx->panel_rev = PANEL_REV_PROTO1_1;
		break;
	case 8:
		ctx->panel_rev = PANEL_REV_EVT1;
		break;
	case 9:
		ctx->panel_rev = PANEL_REV_EVT1_1;
		break;
	case 0x10:
		ctx->panel_rev = PANEL_REV_DVT1;
		break;
	case 0x11:
		ctx->panel_rev = PANEL_REV_DVT1_1;
		break;
	case 0x20:
		ctx->panel_rev = PANEL_REV_PVT;
		break;
	default:
		dev_warn(ctx->dev,
			 "unknown rev from panel (0x%x), default to latest\n",
			 rev);
		ctx->panel_rev = PANEL_REV_LATEST;
		return;
	}

	dev_info(ctx->dev, "panel_rev: 0x%x\n", ctx->panel_rev);
}

static const struct exynos_display_underrun_param underrun_param = {
	.te_idle_us = 1000,
	.te_var = 1,
};

static const u32 s6e3fc3_6a_bl_range[] = {
	95, 205, 315, 400, 2047
};

static const struct exynos_panel_mode s6e3fc3_6a_modes[] = {
	{
		/* 1080x2400 @ 60Hz */
		.mode = {
			.clock = 168498,
			.hdisplay = 1080,
			.hsync_start = 1080 + 32, // add hfp
			.hsync_end = 1080 + 32 + 12, // add hsa
			.htotal = 1080 + 32 + 12 + 26, // add hbp
			.vdisplay = 2400,
			.vsync_start = 2400 + 12, // add vfp
			.vsync_end = 2400 + 12 + 4, // add vsa
			.vtotal = 2400 + 12 + 4 + 26, // add vbp
			.flags = 0,
			.width_mm = 64,
			.height_mm = 142,
		},
		.exynos_mode = {
			.mode_flags = MIPI_DSI_CLOCK_NON_CONTINUOUS,
			.vblank_usec = 120,
			.bpc = 8,
			.dsc = {
				.enabled = true,
				.dsc_count = 2,
				.slice_count = 2,
				.slice_height = 48,
			},
			.underrun_param = &underrun_param,
		},
		.te2_timing = {
			.rising_edge = 0,
			.falling_edge = 0 + 66,
		},
	},
};

static const struct exynos_panel_mode s6e3fc3_6a_lp_mode = {
	.mode = {
		/* 1080x2400 @ 30Hz */
		.name = "1080x2400x30",
		.clock = 84249,
		.hdisplay = 1080,
		.hsync_start = 1080 + 32, // add hfp
		.hsync_end = 1080 + 32 + 12, // add hsa
		.htotal = 1080 + 32 + 12 + 26, // add hbp
		.vdisplay = 2400,
		.vsync_start = 2400 + 12, // add vfp
		.vsync_end = 2400 + 12 + 4, // add vsa
		.vtotal = 2400 + 12 + 4 + 26, // add vbp
		.flags = 0,
		.type = DRM_MODE_TYPE_DRIVER,
		.width_mm = 64,
		.height_mm = 142,
	},
	.exynos_mode = {
		.mode_flags = MIPI_DSI_CLOCK_NON_CONTINUOUS,
		.vblank_usec = 120,
		.bpc = 8,
		.dsc = {
			.enabled = true,
			.dsc_count = 2,
			.slice_count = 2,
			.slice_height = 48,
		},
		.underrun_param = &underrun_param,
		.is_lp_mode = true,
	}
};

static const struct drm_panel_funcs s6e3fc3_6a_drm_funcs = {
	.disable = exynos_panel_disable,
	.unprepare = exynos_panel_unprepare,
	.prepare = exynos_panel_prepare,
	.enable = s6e3fc3_6a_enable,
	.get_modes = exynos_panel_get_modes,
};

static const struct exynos_panel_funcs s6e3fc3_6a_exynos_funcs = {
	.set_brightness = exynos_panel_set_brightness,
	.set_lp_mode = exynos_panel_set_lp_mode,
	.set_nolp_mode = s6e3fc3_6a_set_nolp_mode,
	.set_binned_lp = exynos_panel_set_binned_lp,
	.set_hbm_mode = s6e3fc3_6a_set_hbm_mode,
	.set_dimming_on = s6e3fc3_6a_set_dimming_on,
	.set_local_hbm_mode = s6e3fc3_6a_set_local_hbm_mode,
	.is_mode_seamless = s6e3fc3_6a_is_mode_seamless,
	.panel_init = s6e3fc3_6a_panel_init,
	.get_panel_rev = s6e3fc3_6a_get_panel_rev,
	.get_te2_edges = exynos_panel_get_te2_edges,
	.configure_te2_edges = s6e3fc3_6a_configure_te2_edges,
	.update_te2 = s6e3fc3_6a_update_te2_stub,
};

const struct brightness_capability s6e3fc3_6a_brightness_capability = {
	.normal = {
		.nits = {
			.min = 2,
			.max = 800,
		},
		.level = {
			.min = 4,
			.max = 2047,
		},
		.percentage = {
			.min = 0,
			.max = 62,
		},
	},
	.hbm = {
		.nits = {
			.min = 850,
			.max = 1200,
		},
		.level = {
			.min = 2389,
			.max = 4095,
		},
		.percentage = {
			.min = 62,
			.max = 100,
		},
	},
};

const struct exynos_panel_desc samsung_s6e3fc3_6a = {
	.dsc_pps = PPS_SETTING,
	.dsc_pps_len = ARRAY_SIZE(PPS_SETTING),
	.data_lane_cnt = 4,
	.max_brightness = 4095,
	.min_brightness = 4,
	.dft_brightness = 1023,
	.brt_capability = &s6e3fc3_6a_brightness_capability,
	/* supported HDR format bitmask : 1(DOLBY_VISION), 2(HDR10), 3(HLG) */
	.hdr_formats = BIT(1) | BIT(2) | BIT(3),
	.max_luminance = 12000000,
	.max_avg_luminance = 1200000,
	.min_luminance = 5,
	.bl_range = s6e3fc3_6a_bl_range,
	.bl_num_ranges = ARRAY_SIZE(s6e3fc3_6a_bl_range),
	.modes = s6e3fc3_6a_modes,
	.num_modes = ARRAY_SIZE(s6e3fc3_6a_modes),
	.off_cmd_set = &s6e3fc3_6a_off_cmd_set,
	.lp_mode = &s6e3fc3_6a_lp_mode,
	.lp_cmd_set = &s6e3fc3_6a_lp_cmd_set,
	.binned_lp = s6e3fc3_6a_binned_lp,
	.num_binned_lp = ARRAY_SIZE(s6e3fc3_6a_binned_lp),
	.panel_func = &s6e3fc3_6a_drm_funcs,
	.exynos_panel_func = &s6e3fc3_6a_exynos_funcs,
};

static const struct of_device_id exynos_panel_of_match[] = {
	{ .compatible = "samsung,s6e3fc3_6a", .data = &samsung_s6e3fc3_6a },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos_panel_of_match);

static struct mipi_dsi_driver exynos_panel_driver = {
	.probe = exynos_panel_probe,
	.remove = exynos_panel_remove,
	.driver = {
		.name = "panel-samsung-s6e3fc3_6a",
		.of_match_table = exynos_panel_of_match,
	},
};
module_mipi_dsi_driver(exynos_panel_driver);

MODULE_DESCRIPTION("MIPI-DSI based Samsung s6e3fc3_6a panel driver");
MODULE_LICENSE("GPL");
