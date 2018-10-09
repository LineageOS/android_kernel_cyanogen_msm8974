/*
 *  R63319 LCD Effect command list
 *
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
#ifndef MDSS_R63319_H
#define MDSS_R63319_H

#define LCD_PANEL_NAME "R63319_LH600QH1_LGD_1440_2560_5.98"

/*
 * Head commands
 */
static char r63319_addr_mode[]		= {0x36,0x00};
static char r63319_pixel_format[]	= {0x3a,0x70};
static char r63319_mcs_protect_off[]	= {0xb0, 0x04};
static char r63319_nop_command[]	= {0x00};

static struct dsi_cmd_desc r63319_packet_head_cmds[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_addr_mode)}, r63319_addr_mode},
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_pixel_format)}, r63319_pixel_format},
	{{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(r63319_mcs_protect_off)}, r63319_mcs_protect_off},
	{{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(r63319_nop_command)}, r63319_nop_command},
	{{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(r63319_nop_command)}, r63319_nop_command},
};

/*
 * Auto Contrast Optimization
 *   requires cabc level 3
 */
static char r63319_aco0[] = {
	0xba,0x07,0xb0,0x00,0x00,0x00,0x87
};
static char r63319_aco1[] = {
	0xba,0x07,0xa3,0x10,0x04,0x10,0x87
};
static char r63319_aco2[] = {
	0xba,0x07,0xa0,0x16,0x06,0x16,0x87
};
static char r63319_aco3[] = {
	0xba,0x07,0x9c,0x1c,0x08,0x1c,0x87
};
static char r63319_aco4[] = {
	0xba,0x07,0x99,0x22,0x0a,0x22,0x87
};
static char r63319_aco5[] = {
	0xba,0x07,0x96,0x28,0x0c,0x28,0x87
};
static char r63319_aco6[] = {
	0xba,0x07,0x93,0x2e,0x0e,0x2e,0x87
};
static char r63319_aco7[] = {
	0xba,0x07,0x90,0x34,0x10,0x34,0x87
};
static char r63319_aco8[] = {
	0xba,0x07,0x8d,0x3a,0x12,0x3a,0x87
};
static char r63319_aco9[] = {
	0xba,0x07,0x8a,0x40,0x15,0x40,0x87
};
static char r63319_aco10[] = {
	0xba,0x07,0x87,0x46,0x18,0x46,0x87
};
static char r63319_aco11[] = {
	0xba,0x07,0x84,0x4c,0x1b,0x4c,0x87
};
static char r63319_aco12[] = {
	0xba,0x07,0x81,0x52,0x1e,0x52,0x87
};
static char r63319_aco13[] = {
	0xba,0x07,0x7e,0x58,0x27,0x58,0x87
};
static char r63319_aco14[] = {
	0xba,0x07,0x78,0x64,0x30,0x64,0x87
};
static char r63319_aco15[] = {
	0xba,0x07,0x70,0x70,0x3a,0x70,0x87
};

static struct dsi_cmd_desc r63319_aco_levels[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco0)}, r63319_aco0},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco1)}, r63319_aco1},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco2)}, r63319_aco2},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco3)}, r63319_aco3},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco4)}, r63319_aco4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco5)}, r63319_aco5},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco6)}, r63319_aco6},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco7)}, r63319_aco7},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco8)}, r63319_aco8},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco9)}, r63319_aco9},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco10)}, r63319_aco10},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco11)}, r63319_aco11},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco12)}, r63319_aco12},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco13)}, r63319_aco13},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco14)}, r63319_aco14},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco15)}, r63319_aco15},
};

/*
 * Color Temperature Adjustment
 */
static char r63319_cta0[] = {
	0xC8,0x01,0x00,0x06,0xfd,0x05,0xFC,0x00,0x00,0x08,
	0xfc,0xfc,0x9e,0x00,0x00,0xfd,0xfe,0xf9,0x37,0x00
};
static char r63319_cta1[] = {
	0xC8,0x01,0x00,0x05,0xfd,0x05,0xFC,0x00,0x00,0x06,
	0xfd,0xff,0xa6,0x00,0x00,0xfd,0xfe,0xfa,0x54,0x00
};
static char r63319_cta2[] = {
	0xC8,0x01,0x00,0x04,0xfd,0x06,0xFC,0x00,0x00,0x07,
	0xfd,0x06,0xb0,0x00,0x00,0xfd,0xfe,0x01,0x6f,0x00
};
static char r63319_cta3[] = {
	0xC8,0x01,0x00,0x04,0xfd,0x06,0xFC,0x00,0x00,0x06,
	0xfd,0x05,0xbe,0x00,0x00,0xfe,0xfd,0x01,0x8f,0x00
};
static char r63319_cta4[] = {
	0xC8,0x01,0x00,0x05,0xfd,0x05,0xFC,0x00,0x00,0x0a,
	0xfc,0x04,0xc9,0x00,0x00,0x00,0xfe,0x07,0xa2,0x00
};
static char r63319_cta5[] = {
	0xC8,0x01,0x00,0x06,0xfd,0x05,0xFC,0x00,0x00,0x0a,
	0xfc,0x04,0xd2,0x00,0x00,0x00,0xff,0x0d,0xb3,0x00
};
static char r63319_cta6[] = {
	0xC8,0x01,0x00,0x05,0xfd,0x05,0xFC,0x00,0x00,0x09,
	0xfd,0x04,0xda,0x00,0x00,0x00,0xff,0x0b,0xcb,0x00
};
static char r63319_cta7[] = {
	0xC8,0x01,0x00,0x04,0xfd,0x05,0xFC,0x00,0x00,0x09,
	0xfc,0x04,0xe4,0x00,0x00,0x00,0x00,0x0b,0xe1,0x00
};
static char r63319_cta8[] = {
	0xC8,0x01,0x00,0x00,0x00,0x00,0xFC,0x00,0x00,0x00,
	0x00,0x00,0xFC,0x00,0x00,0x00,0x00,0x00,0xFC,0x00
};
static char r63319_cta9[] = {
	0xC8,0x01,0x00,0x06,0xfc,0xfb,0xf7,0x00,0x00,0x08,
	0xfc,0xf9,0xe4,0x00,0x00,0x01,0x00,0x00,0xfc,0x00
};
static char r63319_cta10[] = {
	0xC8,0x01,0x00,0x06,0xfc,0xfa,0xe9,0x00,0x00,0x08,
	0xfc,0xf9,0xd8,0x00,0x00,0x01,0x00,0x00,0xfc,0x00
};
static char r63319_cta11[] = {
	0xC8,0x01,0x00,0x06,0xfc,0xfa,0xdb,0x00,0x00,0x08,
	0xfc,0xf8,0xcc,0x00,0x00,0x01,0x00,0x00,0xfc,0x00
};
static char r63319_cta12[] = {
	0xC8,0x01,0x00,0x06,0xfc,0xfa,0xdb,0x00,0x00,0x08,
	0xfc,0xf8,0xcc,0x00,0x00,0x01,0x00,0x00,0xfc,0x00
};
static char r63319_cta13[] = {
	0xC8,0x01,0x00,0x06,0xfc,0xfc,0xbd,0x00,0x00,0x07,
	0xfe,0xfb,0xb0,0x00,0x00,0x01,0x00,0x00,0xfc,0x00
};
static char r63319_cta14[] = {
	0xC8,0x01,0x00,0x05,0xfe,0xff,0xac,0x00,0x00,0x09,
	0xfd,0xfb,0xa5,0x00,0x00,0x01,0x00,0x00,0xfc,0x00
};
static char r63319_cta15[] = {
	0xC8,0x01,0x00,0x06,0xfd,0xff,0xa9,0x00,0x00,0x07,
	0xfe,0xfb,0xa2,0x00,0x00,0x01,0x00,0x00,0xFC,0x00
};

static struct dsi_cmd_desc r63319_cta_levels[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta0)}, r63319_cta0},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta1)}, r63319_cta1},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta2)}, r63319_cta2},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta3)}, r63319_cta3},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta4)}, r63319_cta4},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta5)}, r63319_cta5},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta6)}, r63319_cta6},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta7)}, r63319_cta7},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta8)}, r63319_cta8},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta9)}, r63319_cta9},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta10)}, r63319_cta10},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta11)}, r63319_cta11},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta12)}, r63319_cta12},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta13)}, r63319_cta13},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta14)}, r63319_cta14},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta15)}, r63319_cta15},
};

/*
 * Content Adaptive Backlight Control
 */
static char r63319_cabc0[] = {0x55, 0x00};
static char r63319_cabc1[] = {0x55, 0x01};
static char r63319_cabc2[] = {0x55, 0x02};
static char r63319_cabc3[] = {0x55, 0x03};

static struct dsi_cmd_desc r63319_cabc_levels[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_cabc0)}, r63319_cabc0}, // off
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_cabc1)}, r63319_cabc1}, // ui
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_cabc2)}, r63319_cabc2}, // picture
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_cabc3)}, r63319_cabc3}, // video
};

/*
 * Sunlight Readability Enhancment
 */
static char r63319_sre0[] = {0x55, 0x70};
static char r63319_sre1[] = {0x55, 0x71};
static char r63319_sre2[] = {0x55, 0x72};
static char r63319_sre3[] = {0x55, 0x73};

static struct dsi_cmd_desc r63319_sre_levels[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_cabc0)}, r63319_cabc0}, // off / no cabc
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_sre0)}, r63319_sre0}, // on / no cabc
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_sre1)}, r63319_sre1}, // ui
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_sre2)}, r63319_sre2}, // picture
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_sre3)}, r63319_sre3}, // video
};

/*
 * Tint
 */
/*
static char r63319_tint_yellow[] = {
	0xc8,0x01,0x8c,0x06,0xfd,0x05,0xfc,0x00,0x8c,0x08,
	0xfc,0xfc,0x9e,0x00,0x8c,0xfd,0xfe,0xf9,0x37,0x00
};
*/

#if 0
/*
 * Normal preset
 */
static struct dsi_cmd_desc r63319_preset_normal[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_cabc3)}, r63319_cabc3},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco7)}, r63319_aco7},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta8)}, r63319_cta8},
};

/*
 * Comfort preset
 */
static struct dsi_cmd_desc r63319_preset_comfort[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_cabc3)}, r63319_cabc3},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco5)}, r63319_aco5},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta5)}, r63319_cta5},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_tint_yellow)}, r63319_tint_yellow},
};

/*
 * Video preset
 */
static struct dsi_cmd_desc r63319_preset_video[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_cabc0)}, r63319_cabc0},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco8)}, r63319_aco8},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta8)}, r63319_cta8},
};

/*
 * Outside preset
 */
static struct dsi_cmd_desc r63319_preset_outside[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(r63319_sre3)}, r63319_sre3},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_aco8)}, r63319_aco8},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(r63319_cta8)}, r63319_cta8},
};
#endif

/**
 * All supported LCD Effects
 */
static unsigned int r63319_aco_default = 7;
static unsigned int r63319_cta_default = 8;
static unsigned int r63319_cabc_default = 3;
static unsigned int r63319_sre_default = 0;

/**
 * LCD Presets [name, bl_gpio, cmds, cnt]
 */
#if 0
static struct mdss_lcd_effect_preset r63319_preset[] = {
	{"custom", 0, r63319_preset_normal, ARRAY_SIZE(r63319_preset_normal)},
	{"normal", 0, r63319_preset_normal, ARRAY_SIZE(r63319_preset_normal)},
	{"comfort", 0, r63319_preset_comfort, ARRAY_SIZE(r63319_preset_comfort)},
	{"video", 0, r63319_preset_video, ARRAY_SIZE(r63319_preset_video)},
	{"outside", 1, r63319_preset_outside, ARRAY_SIZE(r63319_preset_outside)},
};
#endif

#endif
