// SPDX-License-Identifier: GPL-2.0
/*
 * SPI bus driver for the Ingenic XBurst SoCs
 * Copyright (c) 2017-2018 Artur Rojek <contact@artur-rojek.eu>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

#define REG_SSIDR	0x0
#define REG_SSICR0	0x4
#define REG_SSICR1	0x8
#define REG_SSISR	0xc
#define REG_SSIGR	0x18

#define REG_SSICR0_TENDIAN_LSB_MASK	(BIT(18) | BIT(19))
#define REG_SSICR0_RENDIAN_LSB_MASK	(BIT(16) | BIT(17))
#define REG_SSICR0_SSIE			BIT(15)
#define REG_SSICR0_LOOP			BIT(10)
#define REG_SSICR0_EACLRUN		BIT(7)
#define REG_SSICR0_FSEL			BIT(6)
#define REG_SSICR0_TFLUSH		BIT(2)
#define REG_SSICR0_RFLUSH		BIT(1)

#define REG_SSICR1_FLEN_OFFSET		0x3
#define REG_SSICR1_FRMHL_MASK		(BIT(31) | BIT(30))
#define REG_SSICR1_FRMHL		BIT(30)
#define REG_SSICR1_UNFIN		BIT(23)
#define REG_SSICR1_PHA			BIT(1)
#define REG_SSICR1_POL			BIT(0)

#define REG_SSISR_END			BIT(7)
#define REG_SSISR_BUSY			BIT(6)
#define REG_SSISR_TFF			BIT(5)
#define REG_SSISR_RFE			BIT(4)
#define REG_SSISR_UNDR			BIT(1)
#define REG_SSISR_OVER			BIT(0)

struct ingenic_spi {
	struct clk *clk;
	void __iomem *base;
	struct resource *mem_res;
};

static const struct of_device_id spi_ingenic_of_match[] = {
	{ .compatible = "ingenic,ingenic-spi" },
	{}
};
MODULE_DEVICE_TABLE(of, spi_ingenic_of_match);

static int spi_ingenic_wait(struct ingenic_spi *ingenic_spi,
			    unsigned long mask,
			    bool condition)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(10);

	while (!!(readl(ingenic_spi->base + REG_SSISR) & mask) != condition) {
		if (time_after(jiffies, timeout))
			return 1;
	};

	return 0;
}

static int spi_ingenic_wait_for_completion(struct spi_controller *ctlr,
					   struct spi_message *msg)
{
	unsigned long long ms = 8LL * 1000LL * msg->frame_length;
	struct spi_transfer *xfer = list_first_entry(&msg->transfers,
						    struct spi_transfer,
						    transfer_list);

	do_div(ms, xfer->speed_hz);
	ms += ms + 200; /* Some tolerance. */

	if (ms > UINT_MAX)
		ms = UINT_MAX;

	if (!wait_for_completion_timeout(&ctlr->xfer_completion,
					 msecs_to_jiffies(ms)))
		return -ETIMEDOUT;

	return 0;
}

static void spi_ingenic_set_cs(struct spi_device *spi, bool enable)
{
	struct spi_controller *ctlr = spi->controller;
	struct ingenic_spi *ingenic_spi = spi_controller_get_devdata(ctlr);
	u32 val_ssicr0, val_ssicr1, val_ssisr;

	val_ssicr0 = readl(ingenic_spi->base + REG_SSICR0);
	val_ssicr1 = readl(ingenic_spi->base + REG_SSICR1);
	val_ssisr = readl(ingenic_spi->base + REG_SSISR);

	if (enable) {
		writel(val_ssicr1 & ~REG_SSICR1_UNFIN,
		       ingenic_spi->base + REG_SSICR1);
		writel(val_ssisr & ~(REG_SSISR_UNDR | REG_SSISR_OVER),
		       ingenic_spi->base + REG_SSISR);

		spi_ingenic_wait(ingenic_spi, REG_SSISR_END, true);
	} else {
		writel(val_ssicr1 | REG_SSICR1_UNFIN,
		       ingenic_spi->base + REG_SSICR1);
		writel(val_ssicr0 | REG_SSICR0_TFLUSH | REG_SSICR0_RFLUSH,
		       ingenic_spi->base + REG_SSICR0);
		writel(val_ssisr & ~(REG_SSISR_UNDR | REG_SSISR_OVER),
		       ingenic_spi->base + REG_SSISR);
	}
}

static void spi_ingenic_xfer_speed(struct ingenic_spi *ingenic_spi,
				   struct spi_transfer *xfer)
{
	unsigned long clk_hz = clk_get_rate(ingenic_spi->clk);
	u32 cdiv;

	if (xfer->speed_hz >= clk_hz/2)
		cdiv = 0; /* max_speed_hz/2 is the fastest we can go. */
	else if (xfer->speed_hz)
		cdiv = (clk_hz / (xfer->speed_hz * 2)) - 1;
	else
		cdiv = 0xff; /* 0xff is the slowest we can go. */

	writel(cdiv & 0xff, ingenic_spi->base + REG_SSIGR);
}

static void spi_ingenic_dma_complete(void *arg)
{
	struct spi_controller *ctlr = arg;

	complete(&ctlr->xfer_completion);
}

static struct dma_async_tx_descriptor
*spi_ingenic_dma_prepare(struct spi_controller *ctlr,
			 struct spi_device *spi,
			 struct ingenic_spi *ingenic_spi,
			 struct spi_transfer *xfer,
			 enum dma_transfer_direction dir)
{
	struct dma_chan *chan;
	struct dma_slave_config cfg;
	struct scatterlist *sgl;
	struct dma_async_tx_descriptor *txd;
	unsigned int bits = xfer->bits_per_word ?: spi->bits_per_word;
	int nents, ret;

	memset(&cfg, 0, sizeof(cfg));
	cfg.direction = dir;
	cfg.src_addr = ingenic_spi->mem_res->start + REG_SSIDR;
	cfg.src_addr_width = bits > 8 ?
				DMA_SLAVE_BUSWIDTH_2_BYTES :
				DMA_SLAVE_BUSWIDTH_1_BYTE;
	cfg.dst_addr = cfg.src_addr;
	cfg.dst_addr_width = cfg.src_addr_width;

	if (dir == DMA_MEM_TO_DEV) {
		chan = ctlr->dma_tx;
		sgl = xfer->tx_sg.sgl;
		nents = xfer->tx_sg.nents;
	} else if (dir == DMA_DEV_TO_MEM) {
		chan = ctlr->dma_rx;
		sgl = xfer->rx_sg.sgl;
		nents = xfer->rx_sg.nents;
	} else
		return ERR_PTR(-EINVAL);

	ret = dmaengine_slave_config(chan, &cfg);
	if (ret)
	    return ERR_PTR(ret);

	txd = dmaengine_prep_slave_sg(chan, sgl, nents, dir,
				      DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!txd)
		return ERR_PTR(-ENOMEM);

	return txd;
}

static int spi_ingenic_dma_xfer(struct spi_controller *ctlr,
				struct device *dev,
				struct spi_transfer *xfer,
				struct spi_message *msg)
{
	struct ingenic_spi *ingenic_spi = spi_controller_get_devdata(ctlr);
	struct dma_async_tx_descriptor *desc_tx, *desc_rx;
	dma_cookie_t cookie;
	int ret;

	desc_rx = spi_ingenic_dma_prepare(ctlr, msg->spi, ingenic_spi, xfer,
					  DMA_DEV_TO_MEM);
	if (IS_ERR(desc_rx)) {
		dev_err(dev, "DMA RX failed: %ld\n", PTR_ERR(desc_rx));
		return PTR_ERR(desc_rx);
	}

	desc_tx = spi_ingenic_dma_prepare(ctlr, msg->spi, ingenic_spi, xfer,
					  DMA_MEM_TO_DEV);
	if (IS_ERR(desc_tx)) {
		dev_err(dev, "DMA TX failed: %ld\n", PTR_ERR(desc_tx));
		return PTR_ERR(desc_tx);
	}

	if (xfer == list_last_entry(&msg->transfers, struct spi_transfer,
				    transfer_list)) {
		desc_rx->callback = spi_ingenic_dma_complete;
		desc_rx->callback_param = ctlr;
	}

	cookie = dmaengine_submit(desc_rx);
	ret = dma_submit_error(cookie);
	if (ret)
		return ret;

	cookie = dmaengine_submit(desc_tx);
	ret = dma_submit_error(cookie);
	if (ret)
		return ret;

	return 0;
}

static int spi_ingenic_transfer_one_message(struct spi_controller *ctlr,
					    struct spi_message *msg)
{
	struct spi_device *spi = msg->spi;
	struct ingenic_spi *ingenic_spi = spi_controller_get_devdata(ctlr);
	struct spi_transfer *xfer;
	int ret = 0;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		ret = spi_ingenic_dma_xfer(ctlr, &ctlr->dev, xfer, msg);
		if (ret)
			goto end;
	}

	xfer = list_first_entry(&msg->transfers, struct spi_transfer,
				transfer_list);
	spi_ingenic_xfer_speed(ingenic_spi, xfer);
	spi_ingenic_set_cs(spi, false);
	reinit_completion(&ctlr->xfer_completion);
	dma_async_issue_pending(ctlr->dma_rx);
	dma_async_issue_pending(ctlr->dma_tx);

	ret = spi_ingenic_wait_for_completion(ctlr, msg);
	if (ret) {
		dev_err(&spi->dev, "DMA transfer timed out.\n");
		goto end;
	}

end:
	spi_ingenic_set_cs(spi, true);
	spi_finalize_current_message(ctlr);
	msg->status = ret;

	return ret;
}

static int spi_ingenic_rx(struct ingenic_spi *ingenic_spi,
			  void *buf,
			  int count,
			  int bits)
{
	int i;

	for (i = 0; i < count; ++i) {
		if (spi_ingenic_wait(ingenic_spi, REG_SSISR_RFE, false))
			return 1;

		if (buf) {
			u32 val = readl(ingenic_spi->base + REG_SSIDR);

			if (bits <= 8)
				((u8 *)buf)[i] = val;
			else
				((u16 *)buf)[i] = val;
		} else
			readl(ingenic_spi->base + REG_SSIDR);
	}

	return 0;
}

static int spi_ingenic_tx(struct ingenic_spi *ingenic_spi,
			 struct spi_transfer *xfer,
			 int count,
			 int bits)
{
	int i, rx_count;
	void *rx_buf = xfer->rx_buf;

	for (i = 0, rx_count = 0; i < count; ++i, ++rx_count) {
		if (readl(ingenic_spi->base + REG_SSISR) & REG_SSISR_TFF) {
			if (xfer->rx_buf) {
				u8 *buf8 = xfer->rx_buf;
				u16 *buf16 = xfer->rx_buf;

				if (bits <= 8)
					rx_buf = buf8 + (i - rx_count);
				else
					rx_buf = buf16 + (i - rx_count);
			} else
				rx_buf = NULL;

			if (spi_ingenic_rx(ingenic_spi, rx_buf, rx_count, bits))
				return 1;

			rx_count = 0;
		}

		if (xfer->tx_buf) {
			u32 val;

			if (bits <= 8)
				val = ((u8 *)xfer->tx_buf)[i];
			else
				val = ((u16 *)xfer->tx_buf)[i];

			writel(val, ingenic_spi->base + REG_SSIDR);
		} else
			writel(0, ingenic_spi->base + REG_SSIDR);
	}

	if (xfer->rx_buf) {
		u8 *buf8 = xfer->rx_buf;
		u16 *buf16 = xfer->rx_buf;

		if (bits <= 8)
			rx_buf = buf8 + (i - rx_count);
		else
			rx_buf = buf16 + (i - rx_count);
	};

	if (rx_count)
		if (spi_ingenic_rx(ingenic_spi, rx_buf, rx_count, bits))
			return 1;

	return 0;
}

static int spi_ingenic_transfer_one(struct spi_controller *ctlr,
				    struct spi_device *spi,
				    struct spi_transfer *xfer)
{
	struct ingenic_spi *ingenic_spi = spi_controller_get_devdata(ctlr);
	unsigned int bits = xfer->bits_per_word ?: spi->bits_per_word;
	unsigned int count;

	spi_ingenic_xfer_speed(ingenic_spi, xfer);

	if (bits <= 8)
		count = xfer->len;
	else
		count = xfer->len / 2;

	return spi_ingenic_tx(ingenic_spi, xfer, count, bits);
}

static bool spi_ingenic_can_dma(struct spi_controller *ctlr,
				struct spi_device *spi,
				struct spi_transfer *xfer)
{
	return true;
}

static int spi_ingenic_request_dma(struct spi_controller *ctlr,
				   struct device *dev)
{
	ctlr->dma_tx = dma_request_slave_channel(dev, "tx");
	if (!ctlr->dma_tx)
		return -ENODEV;

	ctlr->dma_rx = dma_request_slave_channel(dev, "rx");

	if (!ctlr->dma_rx)
		return -ENODEV;

	ctlr->can_dma = spi_ingenic_can_dma;

	return 0;
}

static void spi_ingenic_release_dma(struct spi_controller *ctlr)
{
	if (ctlr->dma_tx)
		dma_release_channel(ctlr->dma_tx);
	if (ctlr->dma_rx)
		dma_release_channel(ctlr->dma_rx);
}

static int spi_ingenic_setup(struct spi_device *spi)
{
	struct spi_controller *ctlr = spi->controller;
	struct ingenic_spi *ingenic_spi = spi_controller_get_devdata(ctlr);
	u32 val;
	u32 cs = REG_SSICR1_FRMHL << (spi->chip_select - 1);

	if (spi->bits_per_word < 2 || spi->bits_per_word > 16) {
		dev_dbg(&spi->dev, "setup: unsupported SPI word size.\n");
		return -EINVAL;
	}

	val = REG_SSICR0_SSIE | REG_SSICR0_TFLUSH | REG_SSICR0_RFLUSH
		| REG_SSICR0_EACLRUN;

	if (spi->mode & SPI_LSB_FIRST)
		val |= REG_SSICR0_RENDIAN_LSB_MASK | REG_SSICR0_TENDIAN_LSB_MASK;

	if (spi->mode & SPI_LOOP)
		val |= REG_SSICR0_LOOP;

	writel(val, ingenic_spi->base + REG_SSICR0);

	val = readl(ingenic_spi->base + REG_SSICR1) & REG_SSICR1_FRMHL_MASK;
	val |= (spi->bits_per_word - 2) << REG_SSICR1_FLEN_OFFSET;

	if (spi->mode & SPI_CPHA)
		val |= REG_SSICR1_PHA;

	if (spi->mode & SPI_CPOL)
		val |= REG_SSICR1_POL;

	if (spi->mode & SPI_CS_HIGH)
		val |= cs;
	else
		val &= ~cs;

	writel(val, ingenic_spi->base + REG_SSICR1);

	return 0;
}

static void spi_ingenic_cleanup(struct spi_device *spi)
{
	struct spi_controller *ctlr = spi->controller;
	struct ingenic_spi *ingenic_spi = spi_controller_get_devdata(ctlr);

	writel(0, ingenic_spi->base + REG_SSICR0);
	clk_disable_unprepare(ingenic_spi->clk);
}

static int spi_ingenic_probe(struct platform_device *pdev)
{
	struct ingenic_spi *ingenic_spi;
	struct spi_controller *ctlr;
	int ret;

	ctlr = spi_alloc_master(&pdev->dev, sizeof(struct ingenic_spi));
	if (!ctlr) {
		dev_err(&pdev->dev, "Unable to allocate SPI controller.\n");
		return -ENOMEM;
	}

	ingenic_spi = spi_controller_get_devdata(ctlr);
	ingenic_spi->clk = devm_clk_get(&pdev->dev, "spi");
	if (IS_ERR(ingenic_spi->clk)) {
		dev_err(&pdev->dev, "Clock not found.\n");
		ret = PTR_ERR(ingenic_spi->clk);
		goto out_free;
	}

	ingenic_spi->mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ingenic_spi->base = devm_ioremap_resource(&pdev->dev,
						  ingenic_spi->mem_res);
	if (IS_ERR(ingenic_spi->base)) {
		ret = PTR_ERR(ingenic_spi->base);
		goto out_free;
	}

	platform_set_drvdata(pdev, ctlr);

	ctlr->setup = spi_ingenic_setup;
	ctlr->cleanup = spi_ingenic_cleanup;
	ctlr->set_cs = spi_ingenic_set_cs;
	ctlr->transfer_one = spi_ingenic_transfer_one;
	ctlr->mode_bits = SPI_MODE_3 | SPI_LSB_FIRST | SPI_LOOP | SPI_CS_HIGH;
	ctlr->flags = SPI_CONTROLLER_MUST_RX | SPI_CONTROLLER_MUST_TX;
	ctlr->dev.of_node = pdev->dev.of_node;

	ret = clk_prepare_enable(ingenic_spi->clk);
	if (ret)
		goto out_free;

	if (!spi_ingenic_request_dma(ctlr, &pdev->dev))
		ctlr->transfer_one_message = spi_ingenic_transfer_one_message;
	else
		dev_warn(&pdev->dev, "DMA not available.\n");

	ret = devm_spi_register_controller(&pdev->dev, ctlr);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register SPI controller.\n");
		goto out_free_clk;
	}

	return 0;

out_free_clk:
	clk_disable_unprepare(ingenic_spi->clk);
	spi_ingenic_release_dma(ctlr);
out_free:
	spi_controller_put(ctlr);
	return ret;
}

static int spi_ingenic_remove(struct platform_device *pdev)
{
	struct spi_controller *ctlr = platform_get_drvdata(pdev);

	spi_ingenic_release_dma(ctlr);
	spi_controller_put(ctlr);

	return 0;
}

static struct platform_driver spi_ingenic_driver = {
	.driver = {
		.name = "spi-ingenic",
		.of_match_table = spi_ingenic_of_match,
	},
	.probe = spi_ingenic_probe,
	.remove = spi_ingenic_remove,
};

module_platform_driver(spi_ingenic_driver);
