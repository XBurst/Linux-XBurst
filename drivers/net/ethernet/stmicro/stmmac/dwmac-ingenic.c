// SPDX-License-Identifier: GPL-2.0-only
/*
 * dwmac-stm32.c - DWMAC Specific Glue layer for STM32 MCU
 *
 * Copyright (C) STMicroelectronics SA 2017
 * Author:  Alexandre Torgue <alexandre.torgue@st.com> for STMicroelectronics.
 */

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_net.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/stmmac.h>

#include "stmmac_platform.h"

#define MACPHYC_TXCLK_SEL_MASK	GENMASK(31, 31)
#define MACPHYC_PHY_INFT_MASK	GENMASK(2, 0)

enum ingenic_mac_version {
	ID_JZ4775,
	ID_X1000,
	ID_X1830,
};

struct ingenic_mac {
	const struct ingenic_soc_info *soc_info;
	struct device *dev;
	struct regmap *regmap;
};

struct ingenic_soc_info {
	enum ingenic_mac_version version;
	u32 mask;

	int (*set_mode)(struct plat_stmmacenet_data *plat_dat);
	int (*suspend)(struct ingenic_mac *mac);
	void (*resume)(struct ingenic_mac *mac);
};

static int ingenic_mac_init(struct plat_stmmacenet_data *plat_dat)
{
	struct ingenic_mac *mac = plat_dat->bsp_priv;
	int ret;

	if (mac->soc_info->set_mode) {
		ret = mac->soc_info->set_mode(plat_dat);
		if (ret)
			return ret;
	}

	return ret;
}

static int ingenic_mac_set_mode(struct plat_stmmacenet_data *plat_dat)
{
	struct ingenic_mac *mac = plat_dat->bsp_priv;
	int val;

	switch (plat_dat->interface) {
	case PHY_INTERFACE_MODE_MII:
		if (mac->soc_info->version > ID_JZ4775)
			goto unsupported_interface;

		val = (0 << 0);
		pr_debug("MAC PHY Control Register : PHY_INTERFACE_MODE_MII\n");
		break;

	case PHY_INTERFACE_MODE_GMII:
		if (mac->soc_info->version > ID_JZ4775)
			goto unsupported_interface;

		val = (1 << 31) | (0 << 0);
		pr_debug("MAC PHY Control Register : PHY_INTERFACE_MODE_GMII\n");
		break;

	case PHY_INTERFACE_MODE_RMII:
		val = (4 << 0);
		pr_debug("MAC PHY Control Register : PHY_INTERFACE_MODE_RMII\n");
		break;

	case PHY_INTERFACE_MODE_RGMII:
		if (mac->soc_info->version > ID_JZ4775)
			goto unsupported_interface;

		val = (1 << 31) | (1 << 0);
		pr_debug("MAC PHY Control Register : PHY_INTERFACE_MODE_RGMII\n");
		break;

	default:

unsupported_interface:
		dev_err(mac->dev, "unsupported interface %d", plat_dat->interface);
		return -EINVAL;
	}

	/* Update MAC PHY control register */
	return regmap_update_bits(mac->regmap, 0, mac->soc_info->mask, val);
}

static int ingenic_mac_probe(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	struct ingenic_mac *mac;
	const struct ingenic_soc_info *data;
	int ret;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	mac = devm_kzalloc(&pdev->dev, sizeof(*mac), GFP_KERNEL);
	if (!mac) {
		ret = -ENOMEM;
		goto err_remove_config_dt;
	}

	data = of_device_get_match_data(&pdev->dev);
	if (!data) {
		dev_err(&pdev->dev, "no of match data provided\n");
		ret = -EINVAL;
		goto err_remove_config_dt;
	}

	/* Get MAC PHY control register */
	mac->regmap = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "mode-reg");
	if (IS_ERR(mac->regmap)) {
		pr_err("%s: failed to get syscon regmap\n", __func__);
		goto err_remove_config_dt;
	}

	mac->soc_info = data;
	mac->dev = &pdev->dev;

	plat_dat->bsp_priv = mac;

	ret = ingenic_mac_init(plat_dat);
	if (ret)
		goto err_remove_config_dt;

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_remove_config_dt;

	return 0;

err_remove_config_dt:
	stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int ingenic_mac_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct ingenic_mac *mac = priv->plat->bsp_priv;

	int ret;

	ret = stmmac_suspend(dev);

	if (mac->soc_info->suspend)
		ret = mac->soc_info->suspend(mac);

	return ret;
}

static int ingenic_mac_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct ingenic_mac *mac = priv->plat->bsp_priv;
	int ret;

	if (mac->soc_info->resume)
		mac->soc_info->resume(mac);

	ret = ingenic_mac_init(priv->plat);
	if (ret)
		return ret;

	ret = stmmac_resume(dev);

	return ret;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(ingenic_mac_pm_ops,
	ingenic_mac_suspend, ingenic_mac_resume);

static struct ingenic_soc_info jz4775_soc_info = {
	.version = ID_JZ4775,
	.mask = MACPHYC_TXCLK_SEL_MASK | MACPHYC_PHY_INFT_MASK,

	.set_mode = ingenic_mac_set_mode,
};

static struct ingenic_soc_info x1000_soc_info = {
	.version = ID_X1000,

	.set_mode = ingenic_mac_set_mode,
};

static struct ingenic_soc_info x1830_soc_info = {
	.version = ID_X1830,
	.mask = MACPHYC_PHY_INFT_MASK,

	.set_mode = ingenic_mac_set_mode,
};

static const struct of_device_id ingenic_mac_of_matches[] = {
	{ .compatible = "ingenic,jz4775-mac", .data = &jz4775_soc_info },
	{ .compatible = "ingenic,x1000-mac", .data = &x1000_soc_info },
	{ .compatible = "ingenic,x1830-mac", .data = &x1830_soc_info },
	{ }
};
MODULE_DEVICE_TABLE(of, ingenic_mac_of_matches);

static struct platform_driver ingenic_mac_driver = {
	.probe  = ingenic_mac_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		.name           = "ingenic-mac",
		.pm		= &ingenic_mac_pm_ops,
		.of_match_table = ingenic_mac_of_matches,
	},
};
module_platform_driver(ingenic_mac_driver);

MODULE_AUTHOR("Christophe Roullier <christophe.roullier@st.com>");
MODULE_DESCRIPTION("Ingenic SoCs DWMAC specific glue layer");
MODULE_LICENSE("GPL v2");
