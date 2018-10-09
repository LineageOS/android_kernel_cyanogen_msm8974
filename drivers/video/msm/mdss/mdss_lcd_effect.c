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
#include <linux/delay.h>

#include "mdss_lcd_effect.h"
#include "mdss_mdp.h"

/* Display panel */
#include "mdss_r63319.h"

static inline struct mdss_lcd_effect_ctx *get_ctx(struct msm_fb_data_type *mfd)
{
	return mfd->panel_info->lcd_effect;
}

static inline struct mdss_dsi_ctrl_pdata *get_ctrl(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata = dev_get_platdata(&mfd->pdev->dev);
	return container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
}

static inline struct mdss_dsi_ctrl_pdata *get_rctrl(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata = dev_get_platdata(&mfd->pdev->dev);

	if (!pdata || !pdata->next) {
		pr_err("%s: Invalid panel data!\n", __func__);
		return NULL;
	}

	return container_of(pdata->next, struct mdss_dsi_ctrl_pdata,
			panel_data);
}

/**
 * Send an array of dsi_cmd_desc to the display panel
 */
static int mdss_lcd_effect_send_cmds(struct mdss_lcd_effect_ctx *mlc)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = get_ctrl(mlc->mfd);
	struct dcs_cmd_req cmdreq;
	int ret;

	if (!mlc->mfd->panel_power_on) {
		msleep(200);
		if (!mlc->mfd->panel_power_on) {
			return -EPERM;
		}
	}

	if (mlc->mfd->panel_info->cont_splash_enabled) {
		return -EPERM;
	}

	if (ctrl_pdata->shared_pdata.broadcast_enable &&
			ctrl_pdata->ndx == DSI_CTRL_0) {

		struct mdss_dsi_ctrl_pdata *rctrl_pdata = get_rctrl(mlc->mfd);

		if (!rctrl_pdata) {
			return -EFAULT;
		}
		ctrl_pdata = rctrl_pdata;
	}

	pr_debug("%s: sending dsi cmds, len=%d\n", __func__, mlc->cmd_cnt);

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	cmdreq.cmds = mlc->cmd_buf;
	cmdreq.cmds_cnt = mlc->cmd_cnt;
	ret = mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);

	return ret;
}

/**
 * Set bl_outdoor gpio value
 */
static int mdss_lcd_effect_set_bl_gpio(struct msm_fb_data_type *mfd, int level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = get_ctrl(mfd);

	if (!mfd->panel_power_on) {
		msleep(200);
		if (!mfd->panel_power_on) {
			return -EPERM;
		}
	}

	if (!gpio_is_valid(ctrl_pdata->bl_outdoor_gpio)) {
		pr_err("%s: bl_outdoor_gpio [%d] invalid\n",
				__func__, ctrl_pdata->bl_outdoor_gpio);
		return -EINVAL;
	}

	gpio_set_value(ctrl_pdata->bl_outdoor_gpio, level);

	pr_info("%s: bl_outdoor_gpio set level=%d\n",
			__func__, level);

	return 0;
}

/**
 * Update effects
 */
static void mdss_lcd_effect_worker(struct work_struct *work) {
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_lcd_effect_ctx *mlc = NULL;

	mlc = container_of(work, struct mdss_lcd_effect_ctx, update_work);

	if (mlc == NULL)
		return;

	if (mlc->mfd == NULL)
		return;

	ctrl_pdata = get_ctrl(mlc->mfd);
	if (ctrl_pdata == NULL)
		return;

	pinfo = &(ctrl_pdata->panel_data.panel_info);
	if (pinfo == NULL)
		return;

	if (!pinfo->panel_power_on)
		return;

	mutex_lock(&mlc->lock);

	// Reset cmd_buf cnt
	mlc->cmd_cnt = 0;

	// Head
	memcpy(mlc->cmd_buf + mlc->cmd_cnt, mlc->head_cmds,
		mlc->head_cmds_cnt * sizeof (struct dsi_cmd_desc));
	mlc->cmd_cnt += mlc->head_cmds_cnt;

	pr_debug("%s: Head code copied, cmd_cnt=%d",
			__func__, mlc->cmd_cnt);

	// ACO (requires CABC 3)
	if (mlc->aco_levels > 0 && (mlc->update & LCDE_ACO || mlc->update & LCDE_CABC_SRE))
	{
		memcpy(mlc->cmd_buf + mlc->cmd_cnt, &mlc->aco_cmds[mlc->aco], sizeof (struct dsi_cmd_desc));
		mlc->cmd_cnt += 1;
	}

	// CTA
	if (mlc->cta_levels > 0 && mlc->update & LCDE_CTA)
	{
		memcpy(mlc->cmd_buf + mlc->cmd_cnt, &mlc->cta_cmds[mlc->cta], sizeof (struct dsi_cmd_desc));
		mlc->cmd_cnt += 1;
	}

	// CABC/SRE
	if (mlc->update & LCDE_CABC_SRE)
	{
		// only if SRE = on
		if (mlc->sre_levels > 0 && mlc->sre > 0)
		{
			// SRE level = CABC+1
			memcpy(mlc->cmd_buf + mlc->cmd_cnt, &mlc->sre_cmds[mlc->cabc+1],
				sizeof (struct dsi_cmd_desc));
			mlc->cmd_cnt += 1;

			// bl_outdoor -> 1
			mdss_lcd_effect_set_bl_gpio(mlc->mfd, true);
		}
		// if SRE = off, update CABC
		else if (mlc->cabc_levels > 0)
		{
			memcpy(mlc->cmd_buf + mlc->cmd_cnt, &mlc->cabc_cmds[mlc->cabc],
				sizeof (struct dsi_cmd_desc));
			mlc->cmd_cnt += 1;

			// bl_outdoor -> 0
			mdss_lcd_effect_set_bl_gpio(mlc->mfd, false);
		}
	}

	pr_info("%s: aco=%d cta=%d cabc=%d sre=%d\n",
		__func__, mlc->aco, mlc->cta, mlc->cabc, mlc->sre);

	// Send commands
	mdss_lcd_effect_send_cmds(mlc);

	mlc->update = 0;

	mutex_unlock(&mlc->lock);
}


/**
 * Queue work
 */
void mdss_lcd_effect_update(struct mdss_lcd_effect_ctx *mlc, uint32_t update)
{
	mutex_lock(&mlc->lock);
	mlc->update |= update;
	mutex_unlock(&mlc->lock);

	queue_work(mlc->wq, &mlc->update_work);
}


/**
 * Get buf size to allocate
 */
static int mdss_lcd_effect_get_alloc_cnt(struct mdss_lcd_effect_ctx *mlc)
{
	int cnt = 0;

	// Head
	cnt += mlc->head_cmds_cnt;

	// Effects
	if (mlc->aco_levels > 0)
		cnt += 1;
	if (mlc->cta_levels > 0)
		cnt += 1;
	if (mlc->cabc_levels > 0) // SRE or CABC
		cnt += 1;

	pr_debug("%s: cnt=%d\n", __func__, cnt);

	return cnt;
}

/**
 *  SYSFS
 */
static ssize_t mdss_lcd_effect_get_panel_name(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", LCD_PANEL_NAME);
}

static ssize_t mdss_lcd_effect_get_aco(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_lcd_effect_ctx *mlc = get_ctx(mfd);

	return sprintf(buf, "%d\n", mlc->aco);
}

static ssize_t mdss_lcd_effect_set_aco(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int value = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_lcd_effect_ctx *mlc = get_ctx(mfd);

	sscanf(buf, "%du", &value);
	if (value != mlc->aco && value < mlc->aco_levels) {
		mutex_lock(&mlc->lock);
		mlc->aco = value;
		mutex_unlock(&mlc->lock);
		mdss_lcd_effect_update(mlc, LCDE_ACO);
	}

	return count;
}
static ssize_t mdss_lcd_effect_get_cta(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_lcd_effect_ctx *mlc = get_ctx(mfd);

	return sprintf(buf, "%d\n", mlc->cta);
}

static ssize_t mdss_lcd_effect_set_cta(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int value = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_lcd_effect_ctx *mlc = get_ctx(mfd);

	sscanf(buf, "%du", &value);
	if (value != mlc->cta && value < mlc->cta_levels) {
		mutex_lock(&mlc->lock);
		mlc->cta = value;
		mutex_unlock(&mlc->lock);
		mdss_lcd_effect_update(mlc, LCDE_CTA);
	}

	return count;
}
static ssize_t mdss_lcd_effect_get_cabc(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_lcd_effect_ctx *mlc = get_ctx(mfd);

	return sprintf(buf, "%d\n", mlc->cabc);
}

static ssize_t mdss_lcd_effect_set_cabc(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int value = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_lcd_effect_ctx *mlc = get_ctx(mfd);

	sscanf(buf, "%du", &value);
	if (value != mlc->cabc && value < mlc->cabc_levels) {
		mutex_lock(&mlc->lock);
		mlc->cabc = value;
		mutex_unlock(&mlc->lock);
		mdss_lcd_effect_update(mlc, LCDE_CABC_SRE);
	}

	return count;
}
static ssize_t mdss_lcd_effect_get_sre(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_lcd_effect_ctx *mlc = get_ctx(mfd);

	return sprintf(buf, "%d\n", mlc->sre > 0 ? 1 : 0);
}

static ssize_t mdss_lcd_effect_set_sre(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int value = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_lcd_effect_ctx *mlc = get_ctx(mfd);

	sscanf(buf, "%du", &value);
	if (value != mlc->sre && value < mlc->sre_levels) {
		mutex_lock(&mlc->lock);
		mlc->sre = value > 0 ? 1 : 0;
		mutex_unlock(&mlc->lock);
		mdss_lcd_effect_update(mlc, LCDE_CABC_SRE);
	}

	return count;
}

static DEVICE_ATTR(panel_name, S_IRUGO | S_IWUSR | S_IWGRP,
	mdss_lcd_effect_get_panel_name, NULL);
static DEVICE_ATTR(aco, S_IRUGO | S_IWUSR | S_IWGRP,
	mdss_lcd_effect_get_aco, mdss_lcd_effect_set_aco);
static DEVICE_ATTR(cta, S_IRUGO | S_IWUSR | S_IWGRP,
	mdss_lcd_effect_get_cta, mdss_lcd_effect_set_cta);
static DEVICE_ATTR(cabc, S_IRUGO | S_IWUSR | S_IWGRP,
	mdss_lcd_effect_get_cabc, mdss_lcd_effect_set_cabc);
static DEVICE_ATTR(sre, S_IRUGO | S_IWUSR | S_IWGRP,
	mdss_lcd_effect_get_sre, mdss_lcd_effect_set_sre);

int mdss_lcd_effect_create_sysfs(struct msm_fb_data_type *mfd)
{
	int rc = 0;
	struct mdss_lcd_effect_ctx *mlc = get_ctx(mfd);

	if (mlc == NULL)
		return 0;

	rc = sysfs_create_file(&mfd->fbi->dev->kobj, &dev_attr_panel_name.attr);
	if (rc)
		goto sysfs_err;

	if (mlc->aco_levels > 0) {
		rc = sysfs_create_file(&mfd->fbi->dev->kobj, &dev_attr_aco.attr);
		if (rc)
			goto sysfs_err;
	}

	if (mlc->cta_levels > 0) {
		rc = sysfs_create_file(&mfd->fbi->dev->kobj, &dev_attr_cta.attr);
		if (rc)
			goto sysfs_err;
	}

	if (mlc->cabc_levels > 0) {
		rc = sysfs_create_file(&mfd->fbi->dev->kobj, &dev_attr_cabc.attr);
		if (rc)
			goto sysfs_err;
	}

	if (mlc->sre_levels > 0) {
		rc = sysfs_create_file(&mfd->fbi->dev->kobj, &dev_attr_sre.attr);
		if (rc)
			goto sysfs_err;
	}

	mlc->mfd = mfd;

	return rc;

sysfs_err:
	pr_err("%s: sysfs creation failed, rc=%d", __func__, rc);
	return rc;
}


/**
 * Initialize
 */
int mdss_lcd_effect_init(struct mdss_panel_info *pinfo)
{
	struct mdss_lcd_effect_ctx *mlc;

	if (pinfo == NULL)
		return -ENODEV;

	mlc = kzalloc(sizeof(struct mdss_lcd_effect_ctx), GFP_KERNEL);

	mutex_init(&mlc->lock);

	// Init commands
#ifdef MDSS_R63319_H
	mlc->head_cmds = r63319_packet_head_cmds;
	mlc->head_cmds_cnt = ARRAY_SIZE(r63319_packet_head_cmds);

	mlc->aco_cmds = r63319_aco_levels;
	mlc->cta_cmds = r63319_cta_levels;
	mlc->cabc_cmds = r63319_cabc_levels;
	mlc->sre_cmds = r63319_sre_levels;

	mlc->aco_levels = ARRAY_SIZE(r63319_aco_levels);
	mlc->cta_levels = ARRAY_SIZE(r63319_cta_levels);
	mlc->cabc_levels = ARRAY_SIZE(r63319_cabc_levels);
	mlc->sre_levels = ARRAY_SIZE(r63319_sre_levels);

	mlc->aco = r63319_aco_default;
	mlc->cta = r63319_cta_default;
	mlc->cabc = r63319_cabc_default;
	mlc->sre = r63319_sre_default;
#else
	pr_err("%s: No display panel commands found!\n",
				__func__);
	return -ENODEV;
#endif

	mlc->update = 0;

	// Allocate cmd buffer
	mlc->cmd_buf_size = mdss_lcd_effect_get_alloc_cnt(mlc);
	mlc->cmd_buf = kzalloc(sizeof(struct dsi_cmd_desc) * mlc->cmd_buf_size, GFP_KERNEL);
	if (!mlc->cmd_buf) {
		pr_err("%s: Failed to allocate cmd buffer!\n",
				__func__);
		return -ENOMEM;
	}

	// Create wq
	mlc->wq = create_singlethread_workqueue("lcd_effect_wq");
	INIT_WORK(&mlc->update_work, mdss_lcd_effect_worker);

	pinfo->lcd_effect = mlc;

	pr_info("%s: Allocate lcd_effect buffer success\n",
				__func__);

	return 0;
}
