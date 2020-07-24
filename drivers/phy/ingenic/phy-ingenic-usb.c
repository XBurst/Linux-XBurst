// SPDX-License-Identifier: GPL-2.0
/*
 * Ingenic SoCs USB PHY driver
 * Copyright (c) Paul Cercueil <paul@crapouillou.net>
 * Copyright (c) 漆鹏振 (Qi Pengzhen) <aric.pzqi@ingenic.com>
 * Copyright (c) 周琰杰 (Zhou Yanjie) <zhouyanjie@wanyeetech.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/phy/phy.h>

/* OTGPHY register offsets */
#define REG_USBPCR_OFFSET			0x00
#define REG_USBRDT_OFFSET			0x04
#define REG_USBVBFIL_OFFSET			0x08
#define REG_USBPCR1_OFFSET			0x0c

/* bits within the USBPCR register */
#define USBPCR_USB_MODE				BIT(31)
#define USBPCR_AVLD_REG				BIT(30)
#define USBPCR_COMMONONN			BIT(25)
#define USBPCR_VBUSVLDEXT			BIT(24)
#define USBPCR_VBUSVLDEXTSEL		BIT(23)
#define USBPCR_POR					BIT(22)
#define USBPCR_SIDDQ				BIT(21)
#define USBPCR_OTG_DISABLE			BIT(20)
#define USBPCR_TXPREEMPHTUNE		BIT(6)

#define USBPCR_IDPULLUP_LSB	28
#define USBPCR_IDPULLUP_MASK		GENMASK(29, USBPCR_IDPULLUP_LSB)
#define USBPCR_IDPULLUP_ALWAYS		(0x2 << USBPCR_IDPULLUP_LSB)
#define USBPCR_IDPULLUP_SUSPEND		(0x1 << USBPCR_IDPULLUP_LSB)
#define USBPCR_IDPULLUP_OTG			(0x0 << USBPCR_IDPULLUP_LSB)

#define USBPCR_COMPDISTUNE_LSB		17
#define USBPCR_COMPDISTUNE_MASK		GENMASK(19, USBPCR_COMPDISTUNE_LSB)
#define USBPCR_COMPDISTUNE_DFT		(0x4 << USBPCR_COMPDISTUNE_LSB)

#define USBPCR_OTGTUNE_LSB			14
#define USBPCR_OTGTUNE_MASK			GENMASK(16, USBPCR_OTGTUNE_LSB)
#define USBPCR_OTGTUNE_DFT			(0x4 << USBPCR_OTGTUNE_LSB)

#define USBPCR_SQRXTUNE_LSB	11
#define USBPCR_SQRXTUNE_MASK		GENMASK(13, USBPCR_SQRXTUNE_LSB)
#define USBPCR_SQRXTUNE_DCR_20PCT	(0x7 << USBPCR_SQRXTUNE_LSB)
#define USBPCR_SQRXTUNE_DFT			(0x3 << USBPCR_SQRXTUNE_LSB)

#define USBPCR_TXFSLSTUNE_LSB		7
#define USBPCR_TXFSLSTUNE_MASK		GENMASK(10, USBPCR_TXFSLSTUNE_LSB)
#define USBPCR_TXFSLSTUNE_DCR_50PPT	(0xf << USBPCR_TXFSLSTUNE_LSB)
#define USBPCR_TXFSLSTUNE_DCR_25PPT	(0x7 << USBPCR_TXFSLSTUNE_LSB)
#define USBPCR_TXFSLSTUNE_DFT		(0x3 << USBPCR_TXFSLSTUNE_LSB)
#define USBPCR_TXFSLSTUNE_INC_25PPT	(0x1 << USBPCR_TXFSLSTUNE_LSB)
#define USBPCR_TXFSLSTUNE_INC_50PPT	(0x0 << USBPCR_TXFSLSTUNE_LSB)

#define USBPCR_TXHSXVTUNE_LSB		4
#define USBPCR_TXHSXVTUNE_MASK		GENMASK(5, USBPCR_TXHSXVTUNE_LSB)
#define USBPCR_TXHSXVTUNE_DFT		(0x3 << USBPCR_TXHSXVTUNE_LSB)
#define USBPCR_TXHSXVTUNE_DCR_15MV	(0x1 << USBPCR_TXHSXVTUNE_LSB)

#define USBPCR_TXRISETUNE_LSB		4
#define USBPCR_TXRISETUNE_MASK		GENMASK(5, USBPCR_TXRISETUNE_LSB)
#define USBPCR_TXRISETUNE_DFT		(0x3 << USBPCR_TXRISETUNE_LSB)

#define USBPCR_TXVREFTUNE_LSB		0
#define USBPCR_TXVREFTUNE_MASK		GENMASK(3, USBPCR_TXVREFTUNE_LSB)
#define USBPCR_TXVREFTUNE_INC_25PPT	(0x7 << USBPCR_TXVREFTUNE_LSB)
#define USBPCR_TXVREFTUNE_DFT		(0x5 << USBPCR_TXVREFTUNE_LSB)

/* bits within the USBRDTR register */
#define USBRDT_UTMI_RST				BIT(27)
#define USBRDT_HB_MASK				BIT(26)
#define USBRDT_VBFIL_LD_EN			BIT(25)
#define USBRDT_IDDIG_EN				BIT(24)
#define USBRDT_IDDIG_REG			BIT(23)
#define USBRDT_VBFIL_EN				BIT(2)

/* bits within the USBPCR1 register */
#define USBPCR1_BVLD_REG			BIT(31)
#define USBPCR1_DPPD				BIT(29)
#define USBPCR1_DMPD				BIT(28)
#define USBPCR1_USB_SEL				BIT(28)
#define USBPCR1_WORD_IF_16BIT		BIT(19)

enum ingenic_usb_phy_version {
	ID_JZ4770,
	ID_JZ4780,
	ID_X1000,
	ID_X1830,
};

struct ingenic_soc_info {
	enum ingenic_usb_phy_version version;

	void (*usb_phy_init)(struct phy *phy);
};

struct ingenic_usb_phy {
	const struct ingenic_soc_info *soc_info;

	struct phy *phy;
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
	struct regulator *vcc_supply;
};

static int ingenic_usb_phy_init(struct phy *phy)
{
	struct ingenic_usb_phy *priv = phy_get_drvdata(phy);
	int err;

	err = clk_prepare_enable(priv->clk);
	if (err) {
		dev_err(priv->dev, "Unable to start clock: %d\n", err);
		return err;
	}

	priv->soc_info->usb_phy_init(phy);

	return 0;
}

static int ingenic_usb_phy_exit(struct phy *phy)
{
	struct ingenic_usb_phy *priv = phy_get_drvdata(phy);

	clk_disable_unprepare(priv->clk);
	regulator_disable(priv->vcc_supply);

	return 0;
}

static int ingenic_usb_phy_power_on(struct phy *phy)
{
	struct ingenic_usb_phy *priv = phy_get_drvdata(phy);
	int err;

	err = regulator_enable(priv->vcc_supply);
	if (err) {
		dev_err(priv->dev, "Unable to enable VCC: %d\n", err);
		return err;
	}

	return 0;
}

static int ingenic_usb_phy_power_off(struct phy *phy)
{
	struct ingenic_usb_phy *priv = phy_get_drvdata(phy);

	regulator_disable(priv->vcc_supply);

	return 0;
}

static int ingenic_usb_phy_set_mode(struct phy *phy,
				  enum phy_mode mode, int submode)
{
	struct ingenic_usb_phy *priv = phy_get_drvdata(phy);
	u32 reg;

	switch (mode) {
	case PHY_MODE_USB_HOST:
		reg = readl(priv->base + REG_USBPCR_OFFSET);
		reg &= ~(USBPCR_VBUSVLDEXT | USBPCR_VBUSVLDEXTSEL | USBPCR_OTG_DISABLE);
		reg |= USBPCR_USB_MODE;
		writel(reg, priv->base + REG_USBPCR_OFFSET);

		break;
	case PHY_MODE_USB_DEVICE:
		if (priv->soc_info->version >= ID_X1000) {
			reg = readl(priv->base + REG_USBPCR1_OFFSET);
			reg |= USBPCR1_BVLD_REG;
			writel(reg, priv->base + REG_USBPCR1_OFFSET);
		}

		reg = readl(priv->base + REG_USBPCR_OFFSET);
		reg &= ~USBPCR_USB_MODE;
		reg |= USBPCR_VBUSVLDEXT | USBPCR_VBUSVLDEXTSEL | USBPCR_OTG_DISABLE;
		writel(reg, priv->base + REG_USBPCR_OFFSET);

		break;
	case PHY_MODE_USB_OTG:
		reg = readl(priv->base + REG_USBPCR_OFFSET);
		reg &= ~USBPCR_OTG_DISABLE;
		reg |= USBPCR_VBUSVLDEXT | USBPCR_VBUSVLDEXTSEL | USBPCR_USB_MODE;
		writel(reg, priv->base + REG_USBPCR_OFFSET);

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct phy_ops ingenic_usb_phy_ops = {
	.init		= ingenic_usb_phy_init,
	.exit		= ingenic_usb_phy_exit,
	.power_on	= ingenic_usb_phy_power_on,
	.power_off	= ingenic_usb_phy_power_off,
	.set_mode	= ingenic_usb_phy_set_mode,
	.owner		= THIS_MODULE,
};

static void jz4770_usb_phy_init(struct phy *phy)
{
	struct ingenic_usb_phy *priv = phy_get_drvdata(phy);
	u32 reg;

	reg = USBPCR_AVLD_REG | USBPCR_COMMONONN | USBPCR_IDPULLUP_ALWAYS |
		USBPCR_COMPDISTUNE_DFT | USBPCR_OTGTUNE_DFT | USBPCR_SQRXTUNE_DFT |
		USBPCR_TXFSLSTUNE_DFT | USBPCR_TXRISETUNE_DFT | USBPCR_TXVREFTUNE_DFT |
		USBPCR_POR;
	writel(reg, priv->base + REG_USBPCR_OFFSET);

	/* Wait for PHY to reset */
	usleep_range(30, 300);
	writel(reg & ~USBPCR_POR, priv->base + REG_USBPCR_OFFSET);
	usleep_range(300, 1000);
}

static void jz4780_usb_phy_init(struct phy *phy)
{
	struct ingenic_usb_phy *priv = phy_get_drvdata(phy);
	u32 reg;

	reg = readl(priv->base + REG_USBPCR1_OFFSET) | USBPCR1_USB_SEL |
		USBPCR1_WORD_IF_16BIT;
	writel(reg, priv->base + REG_USBPCR1_OFFSET);

	reg = USBPCR_TXPREEMPHTUNE | USBPCR_COMMONONN | USBPCR_POR;
	writel(reg, priv->base + REG_USBPCR_OFFSET);

	/* Wait for PHY to reset */
	usleep_range(30, 300);
	writel(reg & ~USBPCR_POR, priv->base + REG_USBPCR_OFFSET);
	usleep_range(300, 1000);
}

static void x1000_usb_phy_init(struct phy *phy)
{
	struct ingenic_usb_phy *priv = phy_get_drvdata(phy);
	u32 reg;

	reg = readl(priv->base + REG_USBPCR1_OFFSET) | USBPCR1_WORD_IF_16BIT;
	writel(reg, priv->base + REG_USBPCR1_OFFSET);

	reg = USBPCR_SQRXTUNE_DCR_20PCT | USBPCR_TXPREEMPHTUNE |
		USBPCR_TXHSXVTUNE_DCR_15MV | USBPCR_TXVREFTUNE_INC_25PPT |
		USBPCR_COMMONONN | USBPCR_POR;
	writel(reg, priv->base + REG_USBPCR_OFFSET);

	/* Wait for PHY to reset */
	usleep_range(30, 300);
	writel(reg & ~USBPCR_POR, priv->base + REG_USBPCR_OFFSET);
	usleep_range(300, 1000);
}

static void x1830_usb_phy_init(struct phy *phy)
{
	struct ingenic_usb_phy *priv = phy_get_drvdata(phy);
	u32 reg;

	/* rdt */
	writel(USBRDT_VBFIL_EN | USBRDT_UTMI_RST, priv->base + REG_USBRDT_OFFSET);

	reg = readl(priv->base + REG_USBPCR1_OFFSET) | USBPCR1_WORD_IF_16BIT |
		USBPCR1_DMPD | USBPCR1_DPPD;
	writel(reg, priv->base + REG_USBPCR1_OFFSET);

	reg = USBPCR_IDPULLUP_OTG | USBPCR_VBUSVLDEXT |	USBPCR_TXPREEMPHTUNE |
		USBPCR_COMMONONN | USBPCR_POR;
	writel(reg, priv->base + REG_USBPCR_OFFSET);

	/* Wait for PHY to reset */
	usleep_range(30, 300);
	writel(reg & ~USBPCR_POR, priv->base + REG_USBPCR_OFFSET);
	usleep_range(300, 1000);
}

static const struct ingenic_soc_info jz4770_soc_info = {
	.version = ID_JZ4770,

	.usb_phy_init = jz4770_usb_phy_init,
};

static const struct ingenic_soc_info jz4780_soc_info = {
	.version = ID_JZ4780,

	.usb_phy_init = jz4780_usb_phy_init,
};

static const struct ingenic_soc_info x1000_soc_info = {
	.version = ID_X1000,

	.usb_phy_init = x1000_usb_phy_init,
};

static const struct ingenic_soc_info x1830_soc_info = {
	.version = ID_X1830,

	.usb_phy_init = x1830_usb_phy_init,
};

static int ingenic_usb_phy_probe(struct platform_device *pdev)
{
	struct ingenic_usb_phy *priv;
	struct phy_provider *provider;
	int err;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;

	priv->soc_info = device_get_match_data(&pdev->dev);
	if (!priv->soc_info) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		return -ENODEV;
	}

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base)) {
		dev_err(priv->dev, "Failed to map registers\n");
		return PTR_ERR(priv->base);
	}

	priv->clk = devm_clk_get(priv->dev, NULL);
	if (IS_ERR(priv->clk)) {
		err = PTR_ERR(priv->clk);
		if (err != -EPROBE_DEFER)
			dev_err(priv->dev, "Failed to get clock\n");
		return err;
	}

	priv->vcc_supply = devm_regulator_get(priv->dev, "vcc");
	if (IS_ERR(priv->vcc_supply)) {
		err = PTR_ERR(priv->vcc_supply);
		if (err != -EPROBE_DEFER)
			dev_err(priv->dev, "Failed to get regulator\n");
		return err;
	}

	priv->phy = devm_phy_create(priv->dev, NULL, &ingenic_usb_phy_ops);
	if (IS_ERR(priv)) {
		dev_err(priv->dev, "Failed to create PHY: %ld\n",	PTR_ERR(priv));
		return PTR_ERR(priv);
	}

	provider = devm_of_phy_provider_register(priv->dev, of_phy_simple_xlate);
	if (IS_ERR(provider)) {
		dev_err(priv->dev, "Failed to register PHY provider: %ld\n", PTR_ERR(provider));
		return PTR_ERR(provider);
	}

	platform_set_drvdata(pdev, priv);
	phy_set_drvdata(priv->phy, priv);

	return 0;
}

static int ingenic_usb_phy_remove(struct platform_device *pdev)
{
	struct ingenic_usb_phy *priv = platform_get_drvdata(pdev);

	clk_disable_unprepare(priv->clk);
	regulator_disable(priv->vcc_supply);

	return 0;
}

static const struct of_device_id ingenic_usb_phy_of_matches[] = {
	{ .compatible = "ingenic,jz4770-phy", .data = &jz4770_soc_info },
	{ .compatible = "ingenic,jz4780-phy", .data = &jz4780_soc_info },
	{ .compatible = "ingenic,x1000-phy", .data = &x1000_soc_info },
	{ .compatible = "ingenic,x1830-phy", .data = &x1830_soc_info },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ingenic_usb_phy_of_matches);

static struct platform_driver ingenic_usb_phy_driver = {
	.probe		= ingenic_usb_phy_probe,
	.remove		= ingenic_usb_phy_remove,
	.driver		= {
		.name	= "ingenic-usb-phy",
		.of_match_table = ingenic_usb_phy_of_matches,
	},
};
module_platform_driver(ingenic_usb_phy_driver);

MODULE_AUTHOR("周琰杰 (Zhou Yanjie) <zhouyanjie@wanyeetech.com>");
MODULE_AUTHOR("漆鹏振 (Qi Pengzhen) <aric.pzqi@ingenic.com>");
MODULE_AUTHOR("Paul Cercueil <paul@crapouillou.net>");
MODULE_DESCRIPTION("Ingenic SoCs USB PHY driver");
MODULE_ALIAS("jz4770_phy");
MODULE_LICENSE("GPL");
