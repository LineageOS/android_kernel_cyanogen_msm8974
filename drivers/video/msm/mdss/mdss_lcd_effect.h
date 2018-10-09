/*
 *  LCD Effect control
 *
 * Copyright (c) 2016, Michal Chvíla <electrydev@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef MDSS_LCDE_H
#define MDSS_LCDE_H

#include <linux/moduleparam.h>
#include <linux/gpio.h>
#include "mdss_dsi.h"
#include "mdss_fb.h"
#include "mdss_dsi_cmd.h"

/**
 * Main
 */
struct mdss_lcd_effect_ctx {
	struct msm_fb_data_type *mfd;

	struct dsi_cmd_desc *head_cmds;
	unsigned int head_cmds_cnt;

	struct dsi_cmd_desc *aco_cmds;
	struct dsi_cmd_desc *cta_cmds;
	struct dsi_cmd_desc *cabc_cmds;
	struct dsi_cmd_desc *sre_cmds;

	unsigned int aco_levels;
	unsigned int cta_levels;
	unsigned int cabc_levels;
	unsigned int sre_levels;

	unsigned int aco;
	unsigned int cta;
	unsigned int cabc;
	unsigned int sre; // 0 or 1 (!= sre_cmds[])

	uint32_t update;

	unsigned int cmd_cnt;
	struct dsi_cmd_desc *cmd_buf;
	unsigned int cmd_buf_size;

	struct mutex lock;
	struct work_struct update_work;
	struct workqueue_struct *wq;
};

enum {
	LCDE_ACO		= 0x01,
	LCDE_CTA		= 0x02,
	LCDE_CABC_SRE		= 0x04,
	LCDE_UPDATE_ALL		= 0xFF,
};

/* Sysfs */
int mdss_lcd_effect_create_sysfs(struct msm_fb_data_type *mfd);

/* Initialize */
int mdss_lcd_effect_init(struct mdss_panel_info *pinfo);

/* Update */
void mdss_lcd_effect_update(struct mdss_lcd_effect_ctx *mlc, uint32_t update);

#endif
