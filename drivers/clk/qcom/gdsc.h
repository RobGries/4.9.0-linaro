/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef __QCOM_GDSC_H__
#define __QCOM_GDSC_H__

#include <linux/err.h>
#include <linux/pm_domain.h>

struct clk;
struct regmap;
struct reset_controller_dev;

/**
 * struct gdsc - Globally Distributed Switch Controller
 * @pd: generic power domain
 * @regmap: regmap for MMIO accesses
 * @gdscr: gsdc control register
 * @gds_hw_ctrl: gds_hw_ctrl register
 * @cxcs: offsets of branch registers to toggle mem/periph bits in
 * @cxc_count: number of @cxcs
 * @pwrsts: Possible powerdomain power states
 * @resets: ids of resets associated with this gdsc
 * @reset_count: number of @resets
 * @rcdev: reset controller
 * @clocks: ids of clocks associated with the gdsc
 * @clock_count: number of @clocks
 * @clks: clock pointers to gdsc clocks
 * @root_clock: id of the root clock to be enabled
 * @root_clk: root clk pointer
 */
struct gdsc {
	struct generic_pm_domain	pd;
	struct generic_pm_domain	*parent;
	struct regmap			*regmap;
	unsigned int			gdscr;
	unsigned int			gds_hw_ctrl;
	unsigned int			clamp_io_ctrl;
	unsigned int			*cxcs;
	unsigned int			cxc_count;
/* supported options for pwrsts */
#define PWRSTS_RET			BIT(0)
#define PWRSTS_OFF			BIT(1)
#define PWRSTS_ON			BIT(2)
#define PWRSTS_MAX			3
#define PWRSTS_OFF_ON			(PWRSTS_OFF | PWRSTS_ON)
#define PWRSTS_RET_ON			(PWRSTS_RET | PWRSTS_ON)
#define PWRSTS_OFF_RET_ON		(PWRSTS_OFF | PWRSTS_RET | PWRSTS_ON)
	const u8			pwrsts;
/* supported options for pwrsts_ret */
#define PWRSTS_RET_ALL			0 /* default retains all */
#define PWRSTS_RET_MEM			BIT(0)
#define PWRSTS_RET_PERIPH		BIT(1)
	const u8			pwrsts_ret;
/* supported flags */
#define VOTABLE				BIT(0)
#define CLAMP_IO	BIT(1)
#define HW_CTRL		BIT(2)
	const u8			flags;
	struct reset_controller_dev	*rcdev;
	unsigned int			*resets;
	unsigned int			reset_count;
	unsigned int			*clocks;
	unsigned int			clock_count;
	struct clk			**clks;
	unsigned int			root_clock;
	struct clk			*root_clk;
};

struct gdsc_desc {
	struct device *dev;
	struct gdsc **scs;
	size_t num;
};

#ifdef CONFIG_QCOM_GDSC
int gdsc_register(struct gdsc_desc *desc, struct reset_controller_dev *,
		  struct regmap *);
void gdsc_unregister(struct gdsc_desc *desc);
#else
static inline int gdsc_register(struct gdsc_desc *desc,
				struct reset_controller_dev *rcdev,
				struct regmap *r)
{
	return -ENOSYS;
}

static inline void gdsc_unregister(struct gdsc_desc *desc) {};
#endif /* CONFIG_QCOM_GDSC */
#ifndef CONFIG_PM
struct gdsc_notifier_block {
	struct notifier_block nb;
	unsigned int	*clocks;
	unsigned int	clock_count;
};
void qcom_pm_add_notifier(struct gdsc_notifier_block *);
#endif /* !CONFIG_PM */
#endif /* __QCOM_GDSC_H__ */
