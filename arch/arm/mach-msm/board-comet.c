/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora Forum nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * Alternatively, provided that this notice is retained in full, this software
 * may be relicensed by the recipient under the terms of the GNU General Public
 * License version 2 ("GPL") and only version 2, in which case the provisions of
 * the GPL apply INSTEAD OF those given above.  If the recipient relicenses the
 * software under the GPL, then the identification text in the MODULE_LICENSE
 * macro must be changed to reflect "GPLv2" instead of "Dual BSD/GPL".  Once a
 * recipient changes the license terms to the GPL, subsequent recipients shall
 * not relicense under alternate licensing terms, including the BSD or dual
 * BSD/GPL terms.  In addition, the following license statement immediately
 * below and between the words START and END shall also then apply when this
 * software is relicensed under the GPL:
 *
 * START
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * END
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/android_pmem.h>
#include <linux/bootmem.h>
#include <linux/usb/mass_storage_function.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/mfd/tps65023.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/mach/mmc.h>
#include <mach/vreg.h>
#include <mach/board.h>
#include <mach/sirc.h>
#include <mach/msm_hsusb.h>
#include <mach/msm_touchpad.h>
#include <mach/msm_i2ckbd.h>
#include <mach/msm_serial_hs.h>
#include <mach/msm_spi.h>
#include <linux/spi/spi.h>
#include <mach/s1r72v05.h>

#include "devices.h"
#include "timer.h"
#include "pm.h"

#define TOUCHPAD_SUSPEND	34
#define TOUCHPAD_IRQ            42

#define MSM_PMEM_MDP_SIZE	0x800000
#define MSM_PMEM_ADSP_SIZE	0x2900000
#define MSM_PMEM_GPU1_SIZE	0x800000
#define MSM_FB_SIZE             0x500000
#define MSM_AUDIO_SIZE		0x200000

#define MSM_SMI_BASE		0x2b00000
#define MSM_SMI_SIZE		0x1500000

#define MSM_FB_BASE		MSM_SMI_BASE
#define MSM_PMEM_GPU0_BASE	(MSM_FB_BASE + MSM_FB_SIZE)
#define MSM_PMEM_GPU0_SIZE	(MSM_SMI_SIZE - MSM_FB_SIZE)

#define PMEM_KERNEL_EBI1_SIZE	0x200000

#define COMET_CPLD_START                 0x70004000
#define COMET_CPLD_PER_ENABLE            0x00000010
#define COMET_CPLD_PER_RESET             0x00000018
#define COMET_CPLD_STATUS                0x00000028
#define COMET_CPLD_EXT_PER_ENABLE        0x00000030
#define COMET_CPLD_I2C_ENABLE            0x00000038
#define COMET_CPLD_EXT_PER_RESET         0x00000048
#define COMET_CPLD_VERSION               0x00000058

#define COMET_CPLD_SIZE                  0x00000060
#define COMET_CPLD_STATUS_WVGA           0x0004
#define COMET_CPLD_VERSION_MAJOR         0xFF00
#define COMET_CPLD_PER_ENABLE_HDD        0x1000
#define COMET_CPLD_PER_ENABLE_WVGA       0x0400
#define COMET_CPLD_PER_ENABLE_LVDS       0x0200
#define COMET_CPLD_PER_ENABLE_IDE        0x0080
#define COMET_CPLD_PER_ENABLE_WXGA       0x0040
#define COMET_CPLD_PER_ENABLE_I2C1       0x0010
#define COMET_CPLD_EXT_PER_ENABLE_WXGA   0x0080
#define COMET_CPLD_PER_RESET_IDE         0x0004
#define COMET_CPLD_EXT_PER_ENABLE_I2C1   0x0008

static int                  cpld_version;
static bool                 wvga_present;
static bool                 wxga_present;
static struct comet_cpld_t {
	u16 per_reset_all_reset;
	u16 ext_per_reset_all_reset;
	u16 i2c_enable;
	u16 per_enable_all;
	u16 ext_per_enable_all;
	u16 bt_reset_reg;
	u16 bt_reset_mask;
} comet_cpld[] = {
	[0] = {
		.per_reset_all_reset     = 0x00FF,
		/* enable all peripherals except microphones and */
		/* reset line for i2c touchpad                   */
		.per_enable_all          = 0xFFD8,
		.bt_reset_reg            = 0x0018,
		.bt_reset_mask           = 0x0001,
	},
	[1] = {
		.per_reset_all_reset     = 0x00BF,
		.ext_per_reset_all_reset = 0x0007,
		.i2c_enable              = 0x07F7,
		/* enable all peripherals except microphones and */
		/* displays                                      */
		.per_enable_all          = 0xF9B8,
		.ext_per_enable_all      = 0x007D,
		.bt_reset_reg            = 0x0048,
		.bt_reset_mask           = 0x0004,
	},
};
static struct comet_cpld_t *cpld_info;

static struct resource smc911x_resources[] = {
	[0] = {
		.start  = 0x84000000,
		.end    = 0x84000100,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = MSM_GPIO_TO_INT(156),
		.end    = 156,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device smc911x_device = {
	.name           = "smc911x",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(smc911x_resources),
	.resource       = smc911x_resources,
};

static struct usb_mass_storage_platform_data usb_mass_storage_pdata = {
	.nluns          = 0x02,
	.buf_size       = 16384,
	.vendor         = "GOOGLE",
	.product        = "Mass storage",
	.release        = 0xffff,
};

static struct platform_device mass_storage_device = {
	.name           = "usb_mass_storage",
	.id             = -1,
	.dev            = {
	.platform_data          = &usb_mass_storage_pdata,
	},
};

static struct usb_function_map usb_functions_map[] = {
	{"diag", 0},
	{"adb", 1},
	{"modem", 2},
	{"nmea", 3},
	{"mass_storage", 4},
	{"ethernet", 5},
};

/* dynamic composition */
static struct usb_composition usb_func_composition[] = {
	{
		.product_id         = 0x9012,
		.functions	    = 0x5, /* 0101 */
	},

	{
		.product_id         = 0x9013,
		.functions	    = 0x15, /* 10101 */
	},

	{
		.product_id         = 0x9014,
		.functions	    = 0x30, /* 110000 */
	},

	{
		.product_id         = 0x9016,
		.functions	    = 0xD, /* 01101 */
	},

	{
		.product_id         = 0x9017,
		.functions	    = 0x1D, /* 11101 */
	},

	{
		.product_id         = 0xF000,
		.functions	    = 0x10, /* 10000 */
	},

	{
		.product_id         = 0xF009,
		.functions	    = 0x20, /* 100000 */
	},

	{
		.product_id         = 0x9018,
		.functions	    = 0x1F, /* 011111 */
	},

};

static struct platform_device hs_device = {
	.name   = "msm-handset",
	.id     = -1,
	.dev    = {
		.platform_data = "8k_handset",
	},
};

#ifdef CONFIG_USB_FS_HOST
static int fsusb_gpio_init(void)
{
	int rc;

	/* FSUSB GPIOs */
	rc = gpio_request(139, "fs_dat");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
		       139, rc);
		return rc;
	}
	rc = gpio_request(140, "fs_se0");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
		       140, rc);
		return rc;
	}
	rc = gpio_request(141, "fs_oe_n");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
		       141, rc);
		return rc;
	}
	return 0;
}

static unsigned fsusb_config[] = {
	GPIO_CFG(139, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(140, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(141, 3, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
};

static void msm_fsusb_setup_gpio(unsigned int enable)
{
	int rc, i;

	for (i = 0; i < ARRAY_SIZE(fsusb_config); i++) {
		rc = gpio_tlmm_config(fsusb_config[i],
			enable ? GPIO_ENABLE : GPIO_DISABLE);
		if (rc)
			pr_err("unable to configure fsusb gpios\n");
	}
}
#endif
static struct msm_hsusb_platform_data msm_hsusb_pdata = {
	.version	= 0x0100,
	.phy_info	= (USB_PHY_INTEGRATED | USB_PHY_MODEL_180NM),
	.vendor_id          = 0x5c6,
	.product_name       = "Qualcomm HSUSB Device",
	.serial_number      = "1234567890ABCDEF",
	.manufacturer_name  = "Qualcomm Incorporated",
	.compositions	= usb_func_composition,
	.num_compositions = ARRAY_SIZE(usb_func_composition),
	.function_map   = usb_functions_map,
	.num_functions	= ARRAY_SIZE(usb_functions_map),
	.config_gpio    = NULL,
#ifdef CONFIG_USB_FS_HOST
	.config_fs_gpio = msm_fsusb_setup_gpio,
#endif
};

static struct android_pmem_platform_data android_pmem_kernel_ebi1_pdata = {
	.name = PMEM_KERNEL_EBI1_DATA_NAME,
	/* if no allocator_type, defaults to PMEM_ALLOCATORTYPE_BITMAP,
	 * the only valid choice at this time. The board structure is
	 * set to all zeros by the C runtime initialization and that is now
	 * the enum value of PMEM_ALLOCATORTYPE_BITMAP, now forced to 0 in
	 * include/linux/android_pmem.h.
	 */
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.size = MSM_PMEM_MDP_SIZE,
	.allocator_type = PMEM_ALLOCATORTYPE_BUDDYBESTFIT,
	.cached = 1,
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.size = MSM_PMEM_ADSP_SIZE,
	.allocator_type = PMEM_ALLOCATORTYPE_BUDDYBESTFIT,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_gpu0_pdata = {
	.name = "pmem_gpu0",
	.start = MSM_PMEM_GPU0_BASE,
	.size = MSM_PMEM_GPU0_SIZE,
	.allocator_type = PMEM_ALLOCATORTYPE_ALLORNOTHING,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_gpu1_pdata = {
	.name = "pmem_gpu1",
	.allocator_type = PMEM_ALLOCATORTYPE_BUDDYBESTFIT,
	.cached = 0,
};

static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &android_pmem_pdata },
};

static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};

static struct platform_device android_pmem_gpu0_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &android_pmem_gpu0_pdata },
};

static struct platform_device android_pmem_gpu1_device = {
	.name = "android_pmem",
	.id = 3,
	.dev = { .platform_data = &android_pmem_gpu1_pdata },
};

static struct platform_device android_pmem_kernel_ebi1_device = {
	.name = "android_pmem",
	.id = 4,
	.dev = { .platform_data = &android_pmem_kernel_ebi1_pdata },
};

static struct resource qsd_spi_resources[] = {
	{
		.name   = "spi_irq_in",
		.start	= INT_SPI_INPUT,
		.end	= INT_SPI_INPUT,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_out",
		.start	= INT_SPI_OUTPUT,
		.end	= INT_SPI_OUTPUT,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_err",
		.start	= INT_SPI_ERROR,
		.end	= INT_SPI_ERROR,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_base",
		.start	= 0xA1200000,
		.end	= 0xA1200000 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device qsd_device_spi = {
	.name	        = "spi_qsd",
	.id	        = 0,
	.num_resources	= ARRAY_SIZE(qsd_spi_resources),
	.resource	= qsd_spi_resources,
};

static struct spi_board_info msm_spi_board_info[] __initdata = {
	{
		.modalias	= "bma150",
		.mode		= SPI_MODE_3,
		.irq		= MSM_GPIO_TO_INT(106),
		.bus_num	= 0,
		.chip_select	= 0,
		.max_speed_hz	= 10000000,
	}
};

static unsigned qsd_spi_gpio_config_data[] = {
	GPIO_CFG(17,  1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),  /* SPI_CLK */
	GPIO_CFG(18,  1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),  /* SPI_MOSI */
	GPIO_CFG(19,  1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),  /* SPI_MISO */
	GPIO_CFG(20,  1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),  /* SPI_CS0 */
	GPIO_CFG(106, 0, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),  /* IRQ_CS0 */
};

static int msm_qsd_spi_gpio_config(void)
{
	int i, rc;

	if (gpio_request(17, "spi_clk"))
		pr_err("failed to request gpio spi_clk\n");
	if (gpio_request(18, "spi_mosi"))
		pr_err("failed to request gpio spi_mosi\n");
	if (gpio_request(19, "spi_miso"))
		pr_err("failed to request gpio spi_miso\n");
	if (gpio_request(20, "spi_cs0"))
		pr_err("failed to request gpio spi_cs0\n");
	if (gpio_request(106, "spi_irq_cs0"))
		pr_err("failed to request gpio spi_irq_cs0\n");

	for (i = 0; i < ARRAY_SIZE(qsd_spi_gpio_config_data); i++) {
		rc = gpio_tlmm_config(qsd_spi_gpio_config_data[i], GPIO_ENABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, qsd_spi_gpio_config_data[i], rc);
			return -EIO;
		}
	}

	return 0;
}

static void msm_qsd_spi_gpio_release(void)
{
	gpio_free(17);
	gpio_free(18);
	gpio_free(19);
	gpio_free(20);
	gpio_free(106);
}

static struct msm_spi_platform_data qsd_spi_pdata = {
	.gpio_config  = msm_qsd_spi_gpio_config,
	.gpio_release = msm_qsd_spi_gpio_release
};

static void __init msm_qsd_spi_init(void)
{
	qsd_device_spi.dev.platform_data = &qsd_spi_pdata;
}

static struct resource msm_fb_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	}
};

static int msm_fb_detect_panel(const char *name)
{
	int ret;

	if ((!strcmp(name, "mddi_toshiba_wvga") && wvga_present) ||
	    (!strcmp(name, "lcdc_external") && wvga_present) ||
	    (!strcmp(name, "lcdc_wxga") && wxga_present))
		ret = 0;
	else
		ret = -ENODEV;

	return ret;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_fb_resources),
	.resource       = msm_fb_resources,
	.dev    = {
		.platform_data = &msm_fb_pdata,
	}
};

static int msm_fb_mddi_sel_clk(u32 *clk_rate)
{
	*clk_rate *= 2;
	return 0;
}

static struct mddi_platform_data mddi_pdata = {
	.mddi_sel_clk = msm_fb_mddi_sel_clk,
};

static void __init msm_fb_add_devices(void)
{
	msm_fb_register_device("mdp", 0);
	msm_fb_register_device("pmdh", &mddi_pdata);
	msm_fb_register_device("emdh", &mddi_pdata);
	msm_fb_register_device("tvenc", 0);
	msm_fb_register_device("lcdc", 0);
}

static struct resource msm_audio_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	},
	{
		.name   = "aux_pcm_dout",
		.start  = 68,
		.end    = 68,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_din",
		.start  = 69,
		.end    = 69,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_syncout",
		.start  = 70,
		.end    = 70,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_clkin_a",
		.start  = 71,
		.end    = 71,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device msm_audio_device = {
	.name   = "msm_audio",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_audio_resources),
	.resource       = msm_audio_resources,
};

static void __iomem *comet_cpld_base(void)
{
	static void __iomem *comet_cpld_base_addr;

	if (!comet_cpld_base_addr) {
		if (!request_mem_region(COMET_CPLD_START, COMET_CPLD_SIZE,
					"cpld")) {
			printk(KERN_ERR
			       "%s: request_mem_region for comet cpld failed\n",
			       __func__);
			goto cpld_base_exit;
		}
		comet_cpld_base_addr = ioremap(COMET_CPLD_START,
					       COMET_CPLD_SIZE);
		if (!comet_cpld_base_addr) {
			release_mem_region(COMET_CPLD_START,
					   COMET_CPLD_SIZE);
			printk(KERN_ERR "%s: Could not map comet cpld\n",
			       __func__);
		}
	}
cpld_base_exit:
	return comet_cpld_base_addr;
}

#define S1R72V05_IRQ_GPIO 99

static int comet_init_s1r72v05(void)
{
	int rc;
	char __iomem *cpld_base;
	u16 per_enable;
	u8 irq_gpio = S1R72V05_IRQ_GPIO;

	cpld_base = comet_cpld_base();
	if (!cpld_base)
		return -ENOMEM;
	per_enable = readw(cpld_base + COMET_CPLD_PER_ENABLE);
	per_enable |= COMET_CPLD_PER_ENABLE_HDD | COMET_CPLD_PER_ENABLE_IDE;
	writew(per_enable,
	       cpld_base + COMET_CPLD_PER_ENABLE);
	cpld_info->per_reset_all_reset &= ~COMET_CPLD_PER_RESET_IDE;
	writew(cpld_info->per_reset_all_reset,
	       cpld_base + COMET_CPLD_PER_RESET);

	rc = gpio_request(irq_gpio, "ide_s1r72v05_irq");
	if (rc) {
		pr_err("Failed to request GPIO pin %d (rc=%d)\n",
		       irq_gpio, rc);
		goto err;
	}

	if (gpio_tlmm_config(GPIO_CFG(irq_gpio,
				      0, GPIO_INPUT, GPIO_NO_PULL,
				      GPIO_2MA),
			     GPIO_ENABLE)) {
		printk(KERN_ALERT
		       "s1r72v05: Could not configure IRQ gpio\n");
		goto err;
	}

	if (gpio_configure(irq_gpio, IRQF_TRIGGER_FALLING)) {
		printk(KERN_ALERT
		       "s1r72v05: Could not set IRQ polarity\n");
		goto err;
	}
	return 0;

err:
	gpio_free(irq_gpio);
	return -ENODEV;
}

static struct resource s1r72v05_resources[] = {
	[0] = {
		.start = 0x70000000,
		.end = 0x70000000 + 0xFF,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = MSM_GPIO_TO_INT(S1R72V05_IRQ_GPIO),
		.end = S1R72V05_IRQ_GPIO,
		.flags = IORESOURCE_IRQ,
	},
};

static struct s1r72v05_platform_data s1r72v05_data = {
	.gpio_setup = comet_init_s1r72v05,
};

static struct platform_device s1r72v05_device = {
	.name           = "s1r72v05",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(s1r72v05_resources),
	.resource       = s1r72v05_resources,
	.dev            = {
		.platform_data          = &s1r72v05_data,
	},
};

static struct msm_serial_hs_platform_data msm_uart_dm1_pdata = {
	.wakeup_irq = MSM_GPIO_TO_INT(45),
	.inject_rx_on_wakeup = 1,
	.rx_to_inject = 0x32,
};

static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= 21,
		.end	= 21,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= 29,
		.end	= 29,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= MSM_GPIO_TO_INT(21),
		.end	= MSM_GPIO_TO_INT(21),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

#ifdef CONFIG_BT
static struct platform_device msm_bt_power_device = {
	.name = "bt_power",
};

enum {
	BT_SYSRST,
	BT_WAKE,
	BT_HOST_WAKE,
	BT_PWR_EN,
	BT_RFR,
	BT_CTS,
	BT_RX,
	BT_TX,
	BT_PCM_DOUT,
	BT_PCM_DIN,
	BT_PCM_SYNC,
	BT_PCM_CLK,
};

static unsigned bt_config_power_on[] = {
	GPIO_CFG(29, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* WAKE */
	GPIO_CFG(21, 0, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* HOST_WAKE */
	GPIO_CFG(22, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PWR_EN */
	GPIO_CFG(43, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* RFR */
	GPIO_CFG(44, 2, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* CTS */
	GPIO_CFG(45, 2, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* Rx */
	GPIO_CFG(46, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* Tx */
	GPIO_CFG(68, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_DOUT */
	GPIO_CFG(69, 1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* PCM_DIN */
	GPIO_CFG(70, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_SYNC */
	GPIO_CFG(71, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_CLK */
};
static unsigned bt_config_power_off[] = {
	GPIO_CFG(29, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* WAKE */
	GPIO_CFG(21, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* HOST_WAKE */
	GPIO_CFG(22, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PWR_EN */
	GPIO_CFG(43, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* RFR */
	GPIO_CFG(44, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* CTS */
	GPIO_CFG(45, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* Rx */
	GPIO_CFG(46, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* Tx */
	GPIO_CFG(68, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_DOUT */
	GPIO_CFG(69, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_DIN */
	GPIO_CFG(70, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_SYNC */
	GPIO_CFG(71, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_CLK */
};

static int bluetooth_power(int on)
{
	int pin, rc;
	char __iomem *cpld_base;

	printk(KERN_DEBUG "%s\n", __func__);

	cpld_base = comet_cpld_base();
	if (!cpld_base)
		return -ENOMEM;

	if (on) {
		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_on); pin++) {
			rc = gpio_tlmm_config(bt_config_power_on[pin],
					      GPIO_ENABLE);
			if (rc) {
				printk(KERN_ERR
				       "%s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_config_power_on[pin], rc);
				return -EIO;
			}
		}

		gpio_set_value(22, on); /* PWR_EN */
		writeb(cpld_info->bt_reset_mask,
		       cpld_base + cpld_info->bt_reset_reg); /* SYSRST */

	} else {
		writeb(cpld_info->bt_reset_mask,
		       cpld_base + cpld_info->bt_reset_reg); /* SYSRST */
		gpio_set_value(22, on); /* PWR_EN */

		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_off); pin++) {
			rc = gpio_tlmm_config(bt_config_power_off[pin],
					      GPIO_ENABLE);
			if (rc) {
				printk(KERN_ERR
				       "%s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_config_power_off[pin], rc);
				return -EIO;
			}
		}

	}

	return 0;
}

static void __init bt_power_init(void)
{
	msm_bt_power_device.dev.platform_data = &bluetooth_power;
}
#else
#define bt_power_init(x) do {} while (0)
#endif

static struct platform_device *devices[] __initdata = {
	&msm_fb_device,
	&msm_device_smd,
	&smc911x_device,
	&s1r72v05_device,
	&android_pmem_kernel_ebi1_device,
	&android_pmem_device,
	&android_pmem_adsp_device,
	&android_pmem_gpu0_device,
	&android_pmem_gpu1_device,
	&msm_device_nand,
	&msm_device_hsusb_otg,
	&msm_device_hsusb_peripheral,
	&mass_storage_device,
	&msm_audio_device,
	&msm_device_uart_dm1,
	&msm_bluesleep_device,
#ifdef CONFIG_BT
	&msm_bt_power_device,
#endif
	&msm_device_i2c,
	&qsd_device_spi,
	&msm_device_tssc,
	&hs_device,
};

#ifdef CONFIG_QSD_SVS
#define TPS65023_MAX_DCDC1	1600
#else
#define TPS65023_MAX_DCDC1	CONFIG_QSD_PMIC_DEFAULT_DCDC1
#endif

static int qsd8x50_tps65023_set_dcdc1(int mVolts)
{
	int rc = 0;
#ifdef CONFIG_QSD_SVS
	rc = tps65023_set_dcdc1_level(mVolts);
	/* By default the TPS65023 will be initialized to 1.225V.
	 * So we can safely switch to any frequency within this
	 * voltage even if the device is not probed/ready.
	 */
	if (rc == -ENODEV && mVolts <= CONFIG_QSD_PMIC_DEFAULT_DCDC1)
		rc = 0;
#else
	/* Disallow frequencies not supported in the default PMIC
	 * output voltage.
	 */
	if (mVolts > CONFIG_QSD_PMIC_DEFAULT_DCDC1)
		rc = -EFAULT;
#endif
	return rc;
}

static struct msm_acpu_clock_platform_data comet_clock_data = {
	.acpu_switch_time_us = 20,
	.max_speed_delta_khz = 256000,
	.vdd_switch_time_us = 62,
	.power_collapse_khz = 128000000,
	.wait_for_irq_khz = 128000000,
	.max_vdd = TPS65023_MAX_DCDC1,
	.acpu_set_vdd = qsd8x50_tps65023_set_dcdc1,
};

static void touchpad_gpio_release(void)
{
	gpio_free(TOUCHPAD_IRQ);
	gpio_free(TOUCHPAD_SUSPEND);
}

static int touchpad_gpio_setup(void)
{
	int rc;
	int suspend_pin = TOUCHPAD_SUSPEND;
	int irq_pin = TOUCHPAD_IRQ;
	unsigned suspend_cfg =
		GPIO_CFG(suspend_pin, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);
	unsigned irq_cfg =
		GPIO_CFG(irq_pin, 1, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);

	rc = gpio_request(irq_pin, "msm_touchpad_irq");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
			irq_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_request(suspend_pin, "msm_touchpad_suspend");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
			suspend_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_tlmm_config(suspend_cfg, GPIO_ENABLE);
	if (rc) {
		pr_err("gpio_tlmm_config failed on pin %d (rc=%d)\n",
			suspend_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_tlmm_config(irq_cfg, GPIO_ENABLE);
	if (rc) {
		pr_err("gpio_tlmm_config failed on pin %d (rc=%d)\n",
			irq_pin, rc);
		goto err_gpioconfig;
	}
	return rc;

err_gpioconfig:
	touchpad_gpio_release();
	return rc;
}

static struct msm_touchpad_platform_data msm_touchpad_data = {
	.gpioirq     = TOUCHPAD_IRQ,
	.gpiosuspend = TOUCHPAD_SUSPEND,
	.gpio_setup  = touchpad_gpio_setup,
	.gpio_shutdown = touchpad_gpio_release
};


#define KBD_IRQ 144

static void kbd_gpio_release(void)
{
	gpio_free(KBD_IRQ);
}

static int kbd_gpio_setup(void)
{
	int rc;
	int irqpin = KBD_IRQ;

	unsigned irqcfg =
		GPIO_CFG(irqpin, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA);

	rc = gpio_request(irqpin, "gpio_keybd_irq");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
			irqpin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_tlmm_config(irqcfg, GPIO_ENABLE);
	if (rc) {
		pr_err("gpio_tlmm_config failed on pin %d (rc=%d)\n",
			irqpin, rc);
		goto err_gpioconfig;
	}
	return rc;

err_gpioconfig:
	kbd_gpio_release();
	return rc;
}

/* use CPLD register to toggle keyboard external reset pin */
static void kbd_hwreset(int kbd_mclrpin)
{
	char __iomem *cpld_base;
	int per_en;
	int ext_per_en;

	cpld_base = comet_cpld_base();

	if (!cpld_base)
		return;

	cpld_version = (readw(cpld_base + COMET_CPLD_VERSION)
			& COMET_CPLD_VERSION_MAJOR) >> 8;

	if (cpld_version >= 2) {
		/* COMET2 */
		/* Reset the pin */
		ext_per_en = readw(cpld_base + COMET_CPLD_EXT_PER_ENABLE);
		ext_per_en = ext_per_en & (~COMET_CPLD_EXT_PER_ENABLE_I2C1);
		writew(ext_per_en, cpld_base + COMET_CPLD_EXT_PER_ENABLE);

		msleep(2);

		/* Set the pin */
		ext_per_en = ext_per_en | COMET_CPLD_EXT_PER_ENABLE_I2C1;
		writew(ext_per_en, cpld_base + COMET_CPLD_EXT_PER_ENABLE);
	} else {
		/* COMET1 */
		/* Reset the pin */
		per_en = readw(cpld_base + COMET_CPLD_PER_ENABLE);
		per_en = per_en & (~COMET_CPLD_PER_ENABLE_I2C1);
		writew(per_en, cpld_base + COMET_CPLD_PER_ENABLE);

		msleep(1);

		/* Set the pin */
		per_en = per_en | COMET_CPLD_PER_ENABLE_I2C1;
		writew(per_en, cpld_base + COMET_CPLD_PER_ENABLE);
	}
}

static struct msm_i2ckbd_platform_data msm_kybd_data = {
	.hwrepeat = 0,
	.scanset1 = 1,
	.gpioirq = KBD_IRQ,
	.gpio_setup = kbd_gpio_setup,
	.gpio_shutdown = kbd_gpio_release,
	.hw_reset = kbd_hwreset,
};

static struct i2c_board_info msm_i2c_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("glidesensor", 0x2A),
		.irq           =  MSM_GPIO_TO_INT(TOUCHPAD_IRQ),
		.platform_data = &msm_touchpad_data
	},
	{
		I2C_BOARD_INFO("msm-i2ckbd", 0x3A),
		.type           = "msm-i2ckbd",
		.irq            = MSM_GPIO_TO_INT(KBD_IRQ),
		.platform_data  = &msm_kybd_data
	},
	{
		I2C_BOARD_INFO("tps65023", 0x48),
	},
};

static void __init comet_init_irq(void)
{
	msm_init_irq();
	msm_init_sirc();
}

static void __init comet_init_host(void)
{
	msm_add_host(0, &msm_hsusb_pdata);
#ifdef CONFIG_USB_FS_HOST
	if (fsusb_gpio_init())
		return;
	msm_add_host(1, &msm_hsusb_pdata);
#endif
}
static void sdcc_gpio_init(void)
{
	/* SDC1 GPIOs */
	if (gpio_request(51, "sdc1_data_3"))
		pr_err("failed to request gpio sdc1_data_3\n");
	if (gpio_request(52, "sdc1_data_2"))
		pr_err("failed to request gpio sdc1_data_2\n");
	if (gpio_request(53, "sdc1_data_1"))
		pr_err("failed to request gpio sdc1_data_1\n");
	if (gpio_request(54, "sdc1_data_0"))
		pr_err("failed to request gpio sdc1_data_0\n");
	if (gpio_request(55, "sdc1_cmd"))
		pr_err("failed to request gpio sdc1_cmd\n");
	if (gpio_request(56, "sdc1_clk"))
		pr_err("failed to request gpio sdc1_clk\n");

	/* SDC2 GPIOs */
	if (gpio_request(62, "sdc2_clk"))
		pr_err("failed to request gpio sdc2_clk\n");
	if (gpio_request(63, "sdc2_cmd"))
		pr_err("failed to request gpio sdc2_cmd\n");
	if (gpio_request(64, "sdc2_data_3"))
		pr_err("failed to request gpio sdc2_data_3\n");
	if (gpio_request(65, "sdc2_data_2"))
		pr_err("failed to request gpio sdc2_data_2\n");
	if (gpio_request(66, "sdc2_data_1"))
		pr_err("failed to request gpio sdc2_data_1\n");
	if (gpio_request(67, "sdc2_data_0"))
		pr_err("failed to request gpio sdc2_data_0\n");

	/* SDC3 GPIOs */
	if (gpio_request(88, "sdc3_clk"))
		pr_err("failed to request gpio sdc3_clk\n");
	if (gpio_request(89, "sdc3_cmd"))
		pr_err("failed to request gpio sdc3_cmd\n");
	if (gpio_request(90, "sdc3_data_3"))
		pr_err("failed to request gpio sdc3_data_3\n");
	if (gpio_request(91, "sdc3_data_2"))
		pr_err("failed to request gpio sdc3_data_2\n");
	if (gpio_request(92, "sdc3_data_1"))
		pr_err("failed to request gpio sdc3_data_1\n");
	if (gpio_request(93, "sdc3_data_0"))
		pr_err("failed to request gpio sdc3_data_0\n");

}

static unsigned sdcc_cfg_data[][6] = {
	/* SDC1 configs */
	{
	GPIO_CFG(51, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(52, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(53, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(54, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(55, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(56, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	},
	/* SDC2 configs */
	{
	GPIO_CFG(62, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	GPIO_CFG(63, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(64, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(65, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(66, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(67, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	},
	/* SDC3 configs */
	{
	GPIO_CFG(88, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	GPIO_CFG(89, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(90, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(91, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(92, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(93, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	},
};

static unsigned long vreg_sts, gpio_sts;
static struct vreg *vreg_mmc;

static void msm_sdcc_setup_gpio(int dev_id, unsigned int enable)
{
	int i, rc;

	if (!(test_bit(dev_id, &gpio_sts)^enable))
		return;

	if (enable)
		set_bit(dev_id, &gpio_sts);
	else
		clear_bit(dev_id, &gpio_sts);

	for (i = 0; i < ARRAY_SIZE(sdcc_cfg_data[dev_id - 1]); i++) {
		rc = gpio_tlmm_config(sdcc_cfg_data[dev_id - 1][i],
			enable ? GPIO_ENABLE : GPIO_DISABLE);
		if (rc)
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, sdcc_cfg_data[dev_id - 1][i], rc);
	}
}

static uint32_t msm_sdcc_setup_power(struct device *dv, unsigned int vdd)
{
	int rc = 0;
	struct platform_device *pdev;

	pdev = container_of(dv, struct platform_device, dev);
	msm_sdcc_setup_gpio(pdev->id, !!vdd);

	if (vdd == 0) {
		if (!vreg_sts)
			return 0;

		clear_bit(pdev->id, &vreg_sts);

		if (!vreg_sts) {
			rc = vreg_disable(vreg_mmc);
			if (rc)
				printk(KERN_ERR "%s: return val: %d \n",
					__func__, rc);
		}
		return 0;
	}

	if (!vreg_sts) {
		rc = vreg_set_level(vreg_mmc, 2850);
		if (!rc)
			rc = vreg_enable(vreg_mmc);
		if (rc)
			printk(KERN_ERR "%s: return val: %d \n",
					__func__, rc);
	}
	set_bit(pdev->id, &vreg_sts);
	return 0;
}

static struct mmc_platform_data comet_sdcc_data = {
	.ocr_mask	= MMC_VDD_27_28 | MMC_VDD_28_29,
	.translate_vdd	= msm_sdcc_setup_power,
};

static void __init comet_init_mmc(void)
{
	vreg_mmc = vreg_get(NULL, "gp6");
	if (IS_ERR(vreg_mmc)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_mmc));
		return;
	}
	sdcc_gpio_init();
	msm_add_sdcc(1, &comet_sdcc_data);
	msm_add_sdcc(3, &comet_sdcc_data);
}

static void
msm_i2c_gpio_config(int iface, int config_type)
{
	int gpio_scl;
	int gpio_sda;
	if (iface) {
		gpio_scl = 60;
		gpio_sda = 61;
	} else {
		gpio_scl = 95;
		gpio_sda = 96;
	}
	if (config_type) {
		gpio_tlmm_config(GPIO_CFG(gpio_scl, 1, GPIO_INPUT,
					GPIO_NO_PULL, GPIO_16MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(gpio_sda, 1, GPIO_INPUT,
					GPIO_NO_PULL, GPIO_16MA), GPIO_ENABLE);
	} else {
		gpio_tlmm_config(GPIO_CFG(gpio_scl, 0, GPIO_OUTPUT,
					GPIO_NO_PULL, GPIO_16MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(gpio_sda, 0, GPIO_OUTPUT,
					GPIO_NO_PULL, GPIO_16MA), GPIO_ENABLE);
	}
}

static struct msm_i2c_platform_data msm_i2c_pdata = {
	.clk_freq = 100000,
	.msm_i2c_config_gpio = msm_i2c_gpio_config,
};

static void __init msm_device_i2c_init(void)
{
	if (gpio_request(95, "i2c_pri_clk"))
		pr_err("failed to request gpio i2c_pri_clk\n");
	if (gpio_request(96, "i2c_pri_dat"))
		pr_err("failed to request gpio i2c_pri_dat\n");
	if (gpio_request(60, "i2c_sec_clk"))
		pr_err("failed to request gpio i2c_sec_clk\n");
	if (gpio_request(61, "i2c_sec_dat"))
		pr_err("failed to request gpio i2c_sec_dat\n");

	msm_device_i2c.dev.platform_data = &msm_i2c_pdata;
}

static struct msm_pm_platform_data msm_pm_data[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].supported = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].latency = 8594,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].residency = 23740,

	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].supported = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].latency = 4594,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].residency = 23740,

	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].supported = 1,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].suspend_enabled
		= 1,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].idle_enabled = 0,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency = 443,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].residency = 1098,

	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].supported = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].latency = 2,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].residency = 0,
};

static unsigned msm_uart_csr_code[] = {
	0x22,		/* 	300 bits per second	*/
	0x33,		/* 	600 bits per second	*/
	0x44,		/* 	1200 bits per second	*/
	0x55,		/* 	2400 bits per second	*/
	0x77,		/* 	4800 bits per second	*/
	0x99,		/* 	9600 bits per second	*/
	0xAA,		/* 	14.4K bits per second	*/
	0xBB,		/* 	19.2K bits per second	*/
	0xCC,		/* 	28.8K bits per second	*/
	0xDD,		/* 	38.4K bits per second	*/
	0xEE,		/* 	57.6K bits per second	*/
	0xFF,		/* 	115.2K bits per second	*/
};

static struct msm_serial_platform_data msm_serial_pdata = {
	.uart_csr_code = msm_uart_csr_code,
};

static void __init comet_init(void)
{
	char __iomem *cpld_base;
	int           per_enable;
	int           ext_per_enable;

	cpld_base = comet_cpld_base();

	if (!cpld_base)
		return;

	cpld_version = (readw(cpld_base + COMET_CPLD_VERSION) &
			COMET_CPLD_VERSION_MAJOR) >> 8;
	if (cpld_version >= 2) {
		cpld_info = &comet_cpld[1];
		per_enable = cpld_info->per_enable_all;
		wvga_present = (readw(cpld_base + COMET_CPLD_STATUS)
				& COMET_CPLD_STATUS_WVGA) != 0;
		wxga_present = !wvga_present;
		ext_per_enable = cpld_info->ext_per_enable_all;
		if (wvga_present)
			per_enable |= COMET_CPLD_PER_ENABLE_WVGA;
		else {
			per_enable |= COMET_CPLD_PER_ENABLE_LVDS |
				COMET_CPLD_PER_ENABLE_WXGA;
			ext_per_enable |= COMET_CPLD_EXT_PER_ENABLE_WXGA;
		}
		writew(ext_per_enable,
		       cpld_base + COMET_CPLD_EXT_PER_ENABLE);
		writew(cpld_info->i2c_enable,
		       cpld_base + COMET_CPLD_I2C_ENABLE);
		writew(cpld_info->ext_per_reset_all_reset,
		       cpld_base + COMET_CPLD_EXT_PER_RESET);
	} else {
		cpld_info = &comet_cpld[0];
		wvga_present = 1;
		wxga_present = 0;
		per_enable = cpld_info->per_enable_all;
		smc911x_resources[0].start = 0x90000000;
		smc911x_resources[0].end   = 0x90000100;
	}

	writew(per_enable,
	       cpld_base + COMET_CPLD_PER_ENABLE);
	writew(cpld_info->per_reset_all_reset,
	       cpld_base + COMET_CPLD_PER_RESET);

	msm_acpu_clock_init(&comet_clock_data);

	msm_device_hsusb_peripheral.dev.platform_data = &msm_hsusb_pdata;
	msm_device_uart3.dev.platform_data = &msm_serial_pdata;
	platform_add_devices(devices, ARRAY_SIZE(devices));
	msm_device_uart_dm1.dev.platform_data = &msm_uart_dm1_pdata;
	bt_power_init();
	msm_fb_add_devices();
	comet_init_host();
	comet_init_mmc();
	msm_device_i2c_init();
	msm_qsd_spi_init();
	i2c_register_board_info(0, msm_i2c_board_info,
				ARRAY_SIZE(msm_i2c_board_info));
	spi_register_board_info(msm_spi_board_info,
				ARRAY_SIZE(msm_spi_board_info));
	msm_pm_set_platform_data(msm_pm_data);
}

static void __init comet_allocate_memory_regions(void)
{
	void *addr;
	unsigned long size;

	size = PMEM_KERNEL_EBI1_SIZE;
	addr = alloc_bootmem_aligned(size, 0x100000);
	android_pmem_kernel_ebi1_pdata.start = __pa(addr);
	android_pmem_kernel_ebi1_pdata.size = size;
	printk(KERN_INFO "allocating %lu bytes at %p (%lx physical)"
	       "for pmem kernel ebi1 arena\n", size, addr, __pa(addr));

	addr = alloc_bootmem(android_pmem_pdata.size);
	android_pmem_pdata.start = __pa(addr);

	size = MSM_PMEM_ADSP_SIZE;
	addr = alloc_bootmem(size);
	android_pmem_adsp_pdata.start = __pa(addr);
	android_pmem_adsp_pdata.size = size;
	printk(KERN_INFO "allocating %lu bytes at %p (%lx physical)"
	       "for adsp pmem\n", size, addr, __pa(addr));

	size = MSM_PMEM_GPU1_SIZE;
	addr = alloc_bootmem_aligned(size, 0x100000);
	android_pmem_gpu1_pdata.start = __pa(addr);
	android_pmem_gpu1_pdata.size = size;
	printk(KERN_INFO "allocating %lu bytes at %p (%lx physical)"
	       "for gpu1 pmem\n", size, addr, __pa(addr));

	size = MSM_FB_SIZE;
	addr = (void *)MSM_FB_BASE;
	msm_fb_resources[0].start = (unsigned long)addr;
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	printk(KERN_INFO "using %lu bytes of SMI at %lx physical for fb\n",
	       size, (unsigned long)addr);

	size = MSM_AUDIO_SIZE;
	addr = alloc_bootmem(size);
	msm_audio_resources[0].start = __pa(addr);
	msm_audio_resources[0].end = __pa(addr) + MSM_AUDIO_SIZE;
	printk(KERN_INFO "allocating %lu bytes at %p (%lx physical)"
	       "for audio\n", size, addr, __pa(addr));
}

static void __init comet_map_io(void)
{
	msm_map_comet_io();
	comet_allocate_memory_regions();
	msm_clock_init(msm_clocks_8x50, msm_num_clocks_8x50);
}

MACHINE_START(QSD8X50_COMET, "QCT QSD8x50 Comet")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params = 0x0,
	.map_io = comet_map_io,
	.init_irq = comet_init_irq,
	.init_machine = comet_init,
	.timer = &msm_timer,
MACHINE_END
