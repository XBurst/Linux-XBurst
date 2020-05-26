// SPDX-License-Identifier: GPL-2.0
/*
 * Ingenic XBurst SoC USB PHY driver
 * Copyright (c) Paul Cercueil <paul@crapouillou.net>
 * Copyright (c) qipengzhen <aric.pzqi@ingenic.com>
 * Copyright (c) 周琰杰 (Zhou Yanjie) <zhouyanjie@wanyeetech.com>
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/otg.h>
#include <linux/usb/phy.h>

#define CPM_REG_USBPCR      		0x3c
#define CPM_REG_USBRDT      		0x40
#define CPM_REG_USBVBFIL    		0x44
#define CPM_REG_USBPCR1     		0x48

/*USB Parameter Control Register*/
#define USBPCR_USB_MODE				BIT(31)
#define USBPCR_AVLD_REG				BIT(30)
#define USBPCR_INCR_MASK			BIT(27)
#define USBPCR_COMMONONN			BIT(25)
#define USBPCR_VBUSVLDEXT			BIT(24)
#define USBPCR_VBUSVLDEXTSEL		BIT(23)
#define USBPCR_POR					BIT(22)
#define USBPCR_SIDDQ				BIT(21)
#define USBPCR_OTG_DISABLE			BIT(20)
#define USBPCR_TXPREEMPHTUNE		BIT(6)

#define USBPCR_IDPULLUP_LSB			28
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

#define USBPCR_SQRXTUNE_LSB			11
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

/*USB Reset Detect Timer Register*/
#define USBRDT_UTMI_RST				BIT(27)
#define USBRDT_HB_MASK				BIT(26)
#define USBRDT_VBFIL_LD_EN			BIT(25)
#define USBRDT_IDDIG_EN				BIT(24)
#define USBRDT_IDDIG_REG			BIT(23)
#define USBRDT_VBFIL_EN				BIT(2)

/*USB Parameter Control Register 1*/
#define USBPCR1_BVLD_REG			BIT(31)
#define USBPCR1_DPPD				BIT(29)
#define USBPCR1_DMPD				BIT(28)
#define USBPCR1_WORD_IF_16BIT		BIT(19)

#define USBPCR1_REFCLKSEL_LSB		26
#define USBPCR1_REFCLKSEL_MASK		GENMASK(27, USBPCR1_REFCLKDIV_LSB)
#define USBPCR1_REFCLKSEL_CLKCORE	(0x3 << USBPCR1_REFCLKSEL_LSB)

#define USBPCR1_REFCLKDIV_LSB		24
#define USBPCR1_REFCLKDIV_MASK		GENMASK(25, USBPCR1_REFCLKDIV_LSB)
#define USBPCR1_REFCLKDIV_48M   	(0x2 << USBPCR1_REFCLKDIV_LSB)
#define USBPCR1_REFCLKDIV_24M   	(0x1 << USBPCR1_REFCLKDIV_LSB)
#define USBPCR1_REFCLKDIV_12M   	(0x0 << USBPCR1_REFCLKDIV_LSB)

enum ingenic_usb_phy_version {
	ID_JZ4770,
	ID_X1000,
	ID_X1830,
};

struct ingenic_usb_phy {
	enum ingenic_usb_phy_version version;

	struct usb_phy phy;
	struct usb_otg otg;
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
};

static int ingenic_usb_phy_set_peripheral(struct usb_otg *otg, struct usb_gadget *gadget)
{
	struct ingenic_usb_phy *priv = container_of(otg, struct ingenic_usb_phy, otg);
	u32 reg;

	if (priv->version >= ID_X1000) {
		reg = readl(priv->base + CPM_REG_USBPCR1);
		reg |= USBPCR1_BVLD_REG;
		writel(reg, priv->base + CPM_REG_USBPCR1);
	}

	reg = readl(priv->base + CPM_REG_USBPCR);
	reg &= ~USBPCR_USB_MODE;
	reg |= USBPCR_VBUSVLDEXT | USBPCR_VBUSVLDEXTSEL | USBPCR_OTG_DISABLE;
	writel(reg, priv->base + CPM_REG_USBPCR);

	return 0;
}

static int ingenic_usb_phy_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	struct ingenic_usb_phy *priv = container_of(otg, struct ingenic_usb_phy, otg);
	u32 reg;

	reg = readl(priv->base + CPM_REG_USBPCR);
	reg &= ~(USBPCR_VBUSVLDEXT | USBPCR_VBUSVLDEXTSEL | USBPCR_OTG_DISABLE);
	reg |= USBPCR_USB_MODE;
	writel(reg, priv->base + CPM_REG_USBPCR);

	return 0;
}

static int ingenic_usb_phy_init(struct usb_phy *phy)
{
	struct ingenic_usb_phy *priv = container_of(phy, struct ingenic_usb_phy, phy);
	int err;
	u32 reg;

	err = clk_prepare_enable(priv->clk);
	if (err) {
		dev_err(priv->dev, "Unable to start clock: %d", err);
		return err;
	}

	if (priv->version >= ID_X1830) {
		/* rdt */
		writel(USBRDT_VBFIL_EN | USBRDT_UTMI_RST, priv->base + CPM_REG_USBRDT);

		reg = readl(priv->base + CPM_REG_USBPCR1);
		reg = USBPCR1_WORD_IF_16BIT | USBPCR1_DMPD | USBPCR1_DPPD;
		writel(reg, priv->base + CPM_REG_USBPCR1);

		reg = USBPCR_IDPULLUP_OTG | USBPCR_VBUSVLDEXT | USBPCR_VBUSVLDEXTSEL |
			USBPCR_SQRXTUNE_DCR_20PCT | USBPCR_TXPREEMPHTUNE;
	} else if (priv->version >= ID_X1000) {
		reg = readl(priv->base + CPM_REG_USBPCR1) & ~USBPCR1_REFCLKDIV_MASK;
		reg = USBPCR1_REFCLKSEL_CLKCORE | USBPCR1_REFCLKDIV_24M | USBPCR1_WORD_IF_16BIT;
		writel(reg, priv->base + CPM_REG_USBPCR1);

		reg = USBPCR_SQRXTUNE_DCR_20PCT | USBPCR_TXPREEMPHTUNE |
			USBPCR_TXHSXVTUNE_DCR_15MV | USBPCR_TXVREFTUNE_INC_25PPT;
	} else {
		reg = USBPCR_AVLD_REG | USBPCR_IDPULLUP_ALWAYS |
			USBPCR_COMPDISTUNE_DFT | USBPCR_OTGTUNE_DFT |
			USBPCR_SQRXTUNE_DFT | USBPCR_TXFSLSTUNE_DFT |
			USBPCR_TXRISETUNE_DFT | USBPCR_TXVREFTUNE_DFT;
	}

	reg = USBPCR_COMMONONN | USBPCR_POR;
	writel(reg, priv->base + CPM_REG_USBPCR);

	/**
	 * Power-On Reset(POR)
	 * Function:This customer-specific signal resets all test registers and state machines
	 * in the USB 2.0 nanoPHY.
	 * The POR signal must be asserted for a minimum of 10 μs.
	 * For POR timing information:
	 *
	 * T0: Power-on reset (POR) is initiated. 0 (reference)
	 * T1: T1 indicates when POR can be set to 1’b0. (To provide examples, values for T2 and T3 are also shown
	 * where T1 = T0 + 30 μs.); In general, T1 must be ≥ T0 + 10 μs. T0 + 10 μs ≤ T1
	 * T2: T2 indicates when PHYCLOCK, CLK48MOHCI, and CLK12MOHCI are available at the macro output, based on
	 * the USB 2.0 nanoPHY reference clock source.
	 * Crystal:
	      • When T1 = T0 + 10 μs:
	        T2 < T1 + 805 μs = T0 + 815 μs
	      • When T1 = T0 + 30 μs:
	        T2 < T1 + 805 μs = T0 + 835 μs
	* see "Reset and Power-Saving Signals" on page 60 an “Powering Up and Powering Down the USB 2.0
	* nanoPHY” on page 73.
	*/
	usleep_range(30, 300);
	writel(reg & ~USBPCR_POR, priv->base + CPM_REG_USBPCR);
	usleep_range(300, 1000);

	return 0;
}

static void ingenic_usb_phy_shutdown(struct usb_phy *phy)
{
	struct ingenic_usb_phy *priv = container_of(phy, struct ingenic_usb_phy, phy);

	clk_disable_unprepare(priv->clk);
}

static void ingenic_usb_phy_remove(void *phy)
{
	usb_remove_phy(phy);
}

static const struct of_device_id ingenic_usb_phy_of_matches[] = {
	{ .compatible = "ingenic,jz4770-phy", .data = (void *) ID_JZ4770 },
	{ .compatible = "ingenic,x1000-phy", .data = (void *) ID_X1000 },
	{ .compatible = "ingenic,x1830-phy", .data = (void *) ID_X1830 },
	{ }
};
MODULE_DEVICE_TABLE(of, ingenic_usb_phy_of_matches);

static int ingenic_usb_phy_probe(struct platform_device *pdev)
{
	int err;
	struct ingenic_usb_phy *priv;
	const struct of_device_id *match;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	match = of_match_device(ingenic_usb_phy_of_matches, &pdev->dev);
	if (match)
		priv->version = (enum ingenic_usb_phy_version)match->data;
	else
		return -ENODEV;

	platform_set_drvdata(pdev, priv);
	priv->dev = &pdev->dev;
	priv->phy.dev = &pdev->dev;
	priv->phy.otg = &priv->otg;
	priv->phy.init = ingenic_usb_phy_init;
	priv->phy.shutdown = ingenic_usb_phy_shutdown;

	priv->otg.state = OTG_STATE_UNDEFINED;
	priv->otg.usb_phy = &priv->phy;
	priv->otg.set_host = ingenic_usb_phy_set_host;
	priv->otg.set_peripheral = ingenic_usb_phy_set_peripheral;

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base)) {
		dev_err(&pdev->dev, "Failed to map registers");
		return PTR_ERR(priv->base);
	}

	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk)) {
		err = PTR_ERR(priv->clk);
		if (err != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Failed to get clock");
		return err;
	}

	err = usb_add_phy(&priv->phy, USB_PHY_TYPE_USB2);
	if (err) {
		if (err != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to register PHY");
		return err;
	}

	return devm_add_action_or_reset(&pdev->dev, ingenic_usb_phy_remove, &priv->phy);
}

static struct platform_driver ingenic_usb_phy_driver = {
	.probe		= ingenic_usb_phy_probe,
	.driver		= {
		.name	= "ingenic-usb-phy",
		.of_match_table = of_match_ptr(ingenic_usb_phy_of_matches),
	},
};
module_platform_driver(ingenic_usb_phy_driver);

MODULE_AUTHOR("Paul Cercueil <paul@crapouillou.net>");
MODULE_DESCRIPTION("Ingenic XBurst SoC USB PHY driver");
MODULE_LICENSE("GPL");
