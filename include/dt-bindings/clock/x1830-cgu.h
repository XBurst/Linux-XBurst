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

#define X1830_CLK_EXCLK		0
#define X1830_CLK_RTCLK		1
#define X1830_CLK_APLL		2
#define X1830_CLK_MPLL		3
#define X1830_CLK_EPLL		4
#define X1830_CLK_VPLL		5
#define X1830_CLK_SCLKA		6
#define X1830_CLK_CPUMUX	7
#define X1830_CLK_CPU		8
#define X1830_CLK_L2CACHE	9
#define X1830_CLK_AHB0		10
#define X1830_CLK_AHB2PMUX	11
#define X1830_CLK_AHB2		12
#define X1830_CLK_PCLK		13
#define X1830_CLK_DDR		14
#define X1830_CLK_MAC		15
#define X1830_CLK_MSCMUX	16
#define X1830_CLK_MSC0		17
#define X1830_CLK_MSC1		18
#define X1830_CLK_SSIPLL	19
#define X1830_CLK_SSIMUX	20
#define X1830_CLK_SSI0		21
#define X1830_CLK_SMB0		22
#define X1830_CLK_SMB1		23
#define X1830_CLK_SMB2		24
#define X1830_CLK_UART0		25
#define X1830_CLK_UART1		26
#define X1830_CLK_SSI1		27
#define X1830_CLK_SFC		28
#define X1830_CLK_PDMA		29

#endif /* __DT_BINDINGS_CLOCK_X1830_CGU_H__ */
