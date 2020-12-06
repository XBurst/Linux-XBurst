/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This header provides clock numbers for the ingenic,x2000-cgu DT binding.
 *
 * They are roughly ordered as:
 *   - external clocks
 *   - PLLs
 *   - muxes/dividers in the order they appear in the x2000 programmers manual
 *   - gates in order of their bit in the CLKGR* registers
 */

#ifndef __DT_BINDINGS_CLOCK_X2000_CGU_H__
#define __DT_BINDINGS_CLOCK_X2000_CGU_H__

#define X2000_CLK_EXCLK			0
#define X2000_CLK_RTCLK			1
#define X2000_CLK_APLL			2
#define X2000_CLK_MPLL			3
#define X2000_CLK_EPLL			4
#define X2000_CLK_OTGPHY		5
#define X2000_CLK_SCLKA			6
#define X2000_CLK_I2S0			7
#define X2000_CLK_I2S1			8
#define X2000_CLK_I2S2			9
#define X2000_CLK_I2S3			10
#define X2000_CLK_CPUMUX		11
#define X2000_CLK_CPU			12
#define X2000_CLK_L2CACHE		13
#define X2000_CLK_AHB0			14
#define X2000_CLK_AHB2PMUX		15
#define X2000_CLK_AHB2			16
#define X2000_CLK_PCLK			17
#define X2000_CLK_DDR			18
#define X2000_CLK_ISP			19
#define X2000_CLK_MAC			20
#define X2000_CLK_RSA			21
#define X2000_CLK_SSIPLL		22
#define X2000_CLK_LCD			23
#define X2000_CLK_MSC0			24
#define X2000_CLK_PWM			25
#define X2000_CLK_SFC			26
#define X2000_CLK_CIM			27
#define X2000_CLK_MSC1			28
#define X2000_CLK_MSC2			29
#define X2000_CLK_DMIC_EXCLK	30
#define X2000_CLK_DMIC			31
#define X2000_CLK_EXCLK_DIV512	32
#define X2000_CLK_RTC			33
#define X2000_CLK_EMC			34
#define X2000_CLK_EFUSE			35
#define X2000_CLK_OTG			36
#define X2000_CLK_SCC			37
#define X2000_CLK_I2C0			38
#define X2000_CLK_I2C1			39
#define X2000_CLK_I2C2			40
#define X2000_CLK_I2C3			41
#define X2000_CLK_SADC			42
#define X2000_CLK_UART0			43
#define X2000_CLK_UART1			44
#define X2000_CLK_UART2			45
#define X2000_CLK_DTRNG			46
#define X2000_CLK_TCU			47
#define X2000_CLK_SSI0			48
#define X2000_CLK_OST			49
#define X2000_CLK_PDMA			50
#define X2000_CLK_SSI1			51
#define X2000_CLK_I2C4			52
#define X2000_CLK_I2C5			53
#define X2000_CLK_ISP0			54
#define X2000_CLK_ISP1			55
#define X2000_CLK_HASH			56
#define X2000_CLK_UART3			57
#define X2000_CLK_UART4			58
#define X2000_CLK_UART5			59
#define X2000_CLK_UART6			60
#define X2000_CLK_UART7			61
#define X2000_CLK_UART8			62
#define X2000_CLK_UART9			63
#define X2000_CLK_MAC0			64
#define X2000_CLK_MAC1			65
#define X2000_CLK_INTC			66
#define X2000_CLK_CSI			67
#define X2000_CLK_DSI			68

#endif /* __DT_BINDINGS_CLOCK_X2000_CGU_H__ */
