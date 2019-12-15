/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This header provides clock numbers for the ingenic,x1830-cgu DT binding.
 *
 * They are roughly ordered as:
 *   - external clocks
 *   - PLLs
 *   - muxes/dividers in the order they appear in the x1830 programmers manual
 *   - gates in order of their bit in the CLKGR* registers
 */

#ifndef __DT_BINDINGS_CLOCK_X1830_CGU_H__
#define __DT_BINDINGS_CLOCK_X1830_CGU_H__

#define X1830_CLK_EXCLK			0
#define X1830_CLK_RTCLK			1
#define X1830_CLK_APLL			2
#define X1830_CLK_MPLL			3
#define X1830_CLK_EPLL			4
#define X1830_CLK_VPLL			5
#define X1830_CLK_SCLKA			6
#define X1830_CLK_CPUMUX		7
#define X1830_CLK_CPU			8
#define X1830_CLK_L2CACHE		9
#define X1830_CLK_AHB0			10
#define X1830_CLK_AHB2PMUX		11
#define X1830_CLK_AHB2			12
#define X1830_CLK_PCLK			13
#define X1830_CLK_DDR			14
#define X1830_CLK_MAC			15
#define X1830_CLK_LCD			16
#define X1830_CLK_MSCMUX		17
#define X1830_CLK_MSC0			18
#define X1830_CLK_MSC1			19
#define X1830_CLK_SSIPLL		20
#define X1830_CLK_SSIPLL_DIV2	21
#define X1830_CLK_SSIMUX		22
#define X1830_CLK_EMC			23
#define X1830_CLK_EFUSE			24
#define X1830_CLK_OTG			25
#define X1830_CLK_SSI0			26
#define X1830_CLK_SMB0			27
#define X1830_CLK_SMB1			28
#define X1830_CLK_SMB2			29
#define X1830_CLK_UART0			30
#define X1830_CLK_UART1			31
#define X1830_CLK_SSI1			32
#define X1830_CLK_SFC			33
#define X1830_CLK_PDMA			34
#define X1830_CLK_TCU			35
#define X1830_CLK_DTRNG			36
#define X1830_CLK_OST			37

#endif /* __DT_BINDINGS_CLOCK_X1830_CGU_H__ */
