#ifndef __SDHCI_INGENIC_H__
#define __SDHCI_INGENIC_H__

/**
 * struct sdhci_ingenic_platdata() - Platform device data for ingenic SDHCI
 * @max_width: The maximum number of data bits supported.
 * @host_caps: Standard MMC host capabilities bit field.
 * @host_caps2: The second standard MMC host capabilities bit field.
 *
 * Initialisation data specific to either the machine or the platform
 * for the device driver to use or call-back when configuring gpio or
 * card speed information.
 */
struct sdhci_ingenic_pdata {
	unsigned int    host_caps;
	unsigned int    host_caps2;
	unsigned int    pm_caps;

	unsigned int	pio_mode;
	unsigned int	enable_autocmd12;
};

/**
 * struct sdhci_ingenic - INGENIC SDHCI instance
 * @host: The SDHCI host created
 * @pdev: The platform device we where created from.
 * @ioarea: The resource created when we claimed the IO area.
 * @pdata: The platform data for this controller.
 */
struct sdhci_ingenic {
	struct device			*dev;
	struct sdhci_host		*host;
	struct platform_device		*pdev;
	struct sdhci_ingenic_pdata	*pdata;
	struct clk			*clk_cgu;
	struct clk			*clk_ext;
	struct clk			*parent;
};

#endif	/* __SDHCI_INGENIC_H__ */
