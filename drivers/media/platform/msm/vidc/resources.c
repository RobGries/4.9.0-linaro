/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/iommu.h>
#include <linux/qcom_iommu.h>
#include <linux/of.h>
#include <linux/slab.h>

#include "internal.h"
#include "resources.h"

static const struct load_freq_table freq_table_8916[] = {
	{ 352800, 228570000 },	/* 1920x1088 @ 30 + 1280x720 @ 30 */
	{ 244800, 160000000 },	/* 1920x1088 @ 30 */
	{ 108000, 100000000 },	/* 1280x720 @ 30 */
};

static const struct reg_value_pair reg_preset_8916[] = {
	{ 0xe0020, 0x05555556 },
	{ 0xe0024, 0x05555556 },
	{ 0x80124, 0x00000003 },
};

static struct clock_info clks_8916[] = {
	{ .name = "core_clk",
	  .count = ARRAY_SIZE(freq_table_8916),
	  .load_freq_tbl = freq_table_8916,
	},
	{ .name = "iface_clk", },
	{ .name = "bus_clk", },
};

static int get_clock_table(struct device *dev, struct vidc_resources *res)
{
	struct clock_set *clocks = &res->clock_set;
	unsigned int i;

	clocks->clock_tbl = clks_8916;
	clocks->count = ARRAY_SIZE(clks_8916);

	for (i = 0; i < clocks->count; i++) {
		clks_8916[i].clk = devm_clk_get(dev, clks_8916[i].name);
		if (IS_ERR(clks_8916[i].clk))
			return PTR_ERR(clks_8916[i].clk);
	}

	return 0;
}

int enable_clocks(struct vidc_resources *res)
{
	struct clock_set *clks = &res->clock_set;
	struct clock_info *tbl = clks->clock_tbl;
	int ret, i;

	for (i = 0; i < clks->count; i++) {
		ret = clk_prepare_enable(tbl[i].clk);
		if (ret)
			goto err;
	}

	return 0;
err:
	while (--i)
		clk_disable_unprepare(tbl[i].clk);

	return ret;
}

void disable_clocks(struct vidc_resources *res)
{
	struct clock_set *clks = &res->clock_set;
	struct clock_info *tbl = clks->clock_tbl;
	int i;

	for (i = clks->count - 1; i >= 0; i--)
		clk_disable_unprepare(tbl[i].clk);
}

struct iommu_context {
	const char *name;
	u32 partition_buf_type;
	u32 virt_addr_pool_start;
	u32 virt_addr_pool_size;
	bool is_secure;
};

static const struct iommu_context iommu_ctxs[] = {
	{ .name			= "venus_ns",
	  .partition_buf_type	= 0xfff,
	  /* non-secure addr pool from 1500 MB to 3548 MB */
	  .virt_addr_pool_start	= 0x5dc00000,
	  .virt_addr_pool_size	= 0x80000000,
	  .is_secure		= false,
	},
	{ .name			= "venus_sec_bitstream",
	  .partition_buf_type	= 0x241,
	  /* secure bitstream addr pool from 1200 MB to 1500 MB */
	  .virt_addr_pool_start	= 0x4b000000,
	  .virt_addr_pool_size	= 0x12c00000,
	  .is_secure		= true,
	},
	{ .name			= "venus_sec_pixel",
	  .partition_buf_type	= 0x106,
	  /* secure pixel addr pool from 600 MB to 1200 MB */
	  .virt_addr_pool_start	= 0x25800000,
	  .virt_addr_pool_size	= 0x25800000,
	  .is_secure		= true,
	},
	{ .name			= "venus_sec_non_pixel",
	  .partition_buf_type	= 0x480,
	  /* secure non-pixel addr pool from 16 MB to 584 MB */
	  .virt_addr_pool_start	= 0x01000000,
	  .virt_addr_pool_size	= 0x24800000,
	  .is_secure		= true,
	}
};

int get_platform_resources(struct vidc_core *core)
{
	struct vidc_resources *res = &core->res;
	struct device *dev = core->dev;
	struct device_node *np = dev->of_node;
	const char *hfi_name = NULL, *propname;
	int ret;

	res->load_freq_tbl = freq_table_8916;
	res->load_freq_tbl_size = ARRAY_SIZE(freq_table_8916);

	res->reg_set.reg_tbl = reg_preset_8916;
	res->reg_set.count = ARRAY_SIZE(reg_preset_8916);

	ret = of_property_read_string(np, "qcom,hfi", &hfi_name);
	if (ret) {
		dev_err(dev, "reading hfi type failed\n");
		return ret;
	}

	if (!strcasecmp(hfi_name, "venus"))
		core->hfi_type = VIDC_VENUS;
	else if (!strcasecmp(hfi_name, "q6"))
		core->hfi_type = VIDC_Q6;
	else
		return -EINVAL;

	propname = "qcom,hfi-version";
	ret = of_property_read_string(np, propname, &res->hfi_version);
	if (ret)
		dev_dbg(dev, "legacy HFI packetization\n");

	ret = get_clock_table(dev, res);
	if (ret) {
		dev_err(dev, "load clock table failed (%d)\n", ret);
		return ret;
	}

	ret = of_property_read_u32(np, "qcom,max-hw-load", &res->max_load);
	if (ret) {
		dev_err(dev, "determine max load supported failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

void put_platform_resources(struct vidc_core *core)
{
}
