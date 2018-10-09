/*
 * Copyright (C) 2010 Trusted Logic S.A.
 * Copyright (C) 2012-2013 NXP Semiconductors.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _PN547_H_
#define _PN547_H_

#define PN547_MAGIC 0xE9

/*
 * PN547 power control via ioctl
 * PN547_SET_PWR(0): power off
 * PN547_SET_PWR(1): power on
 * PN547_SET_PWR(2): power on with firmware download enabled
 * PN547_SET_PWR(3): cancel read
 */
#define PN547_SET_PWR _IOW(PN547_MAGIC, 0x01, unsigned int)

#define PN547_DEVICE_NAME "pn547"

enum pn547_set_pwr_cmd {
	PN547_SET_PWR_OFF,
	PN547_SET_PWR_ON,
	PN547_SET_PWR_FWDL,
	PN547_SET_PWR_CANCEL_READ,
};

enum pn547_state {
	PN547_STATE_UNKNOWN,
	PN547_STATE_OFF,
	PN547_STATE_ON,
	PN547_STATE_FWDL,
};

struct pn547_i2c_platform_data {
	int irq_gpio;
	int firm_gpio;
	int ven_gpio;
	int pvdd_en_gpio;
	int (*chip_config)(enum pn547_state, void *);
	int (*driver_loaded)(void);
	void (*driver_unloaded)(void);
	int (*driver_opened)(void);
	void (*driver_closed)(void);
};

#endif

