/*
 * This file is part of the libopencm3 project.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * USB device driver for the STM32H72x/73x/74x/75x USB2 OTG_FS core
 * (PA11 = DM, PA12 = DP, embedded full-speed PHY). Endpoint and FIFO
 * handling is shared with the other DWC drivers in usb_dwc_common.c.
 */

#include <libopencm3/cm3/common.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/bos.h>
#include <libopencm3/usb/dwc/otg_fs.h>
#include "usb_private.h"
#include "usb_dwc_common.h"

/* B-session valid override (not defined by libopencm3's otg_common.h). */
#define OTG_GOTGCTL_BVALOVAL	(1U << 7U)
#define OTG_GOTGCTL_BVALOEN	(1U << 6U)

/* USB turnaround time for an AHB clock of 32 MHz or more (RM0433 Table 351). */
#define OTG_GUSBCFG_TRDT_AHB_32MHZ	(6U << 10U)

/*
 * Receive FIFO size in 32-bit words. The core has 4 KiB (1024 words) of
 * FIFO RAM shared by the RX FIFO and every IN endpoint's TX FIFO.
 */
#define RX_FIFO_SIZE 128U

static usbd_device *stm32h7_otgfs_usbd_init(void);

static usbd_device usbd_dev;

const struct _usbd_driver stm32h7_otgfs_usb_driver = {
	.init = stm32h7_otgfs_usbd_init,
	.set_address = dwc_set_address,
	.ep_setup = dwc_ep_setup,
	.ep_reset = dwc_endpoints_reset,
	.ep_stall_set = dwc_ep_stall_set,
	.ep_stall_get = dwc_ep_stall_get,
	.ep_nak_set = dwc_ep_nak_set,
	.ep_write_packet = dwc_ep_write_packet,
	.ep_read_packet = dwc_ep_read_packet,
	.poll = dwc_poll,
	.disconnect = dwc_disconnect,
	.enable_sof = dwc_enable_sof,
	.base_address = USB_OTG_FS_BASE,
	.set_address_before_status = true,
	.rx_fifo_size = RX_FIFO_SIZE,
};

static usbd_device *stm32h7_otgfs_usbd_init(void)
{
	/* The USB transceiver needs the VDD33USB level detector enabled. */
	pwr_enable_usb33();

	/* RCC_USB2OTGHS is the AHB1ENR USB2OTGFSEN bit. */
	rcc_periph_clock_enable(RCC_USB2OTGHS);

	/* Select the embedded full-speed PHY, then soft-reset the core. */
	OTG_FS_GUSBCFG |= OTG_GUSBCFG_PHYSEL;
	while ((OTG_FS_GRSTCTL & OTG_GRSTCTL_AHBIDL) == 0U)
		continue;
	OTG_FS_GRSTCTL = OTG_GRSTCTL_CSRST;
	while ((OTG_FS_GRSTCTL & OTG_GRSTCTL_CSRST) != 0U)
		continue;

	/* Stay disconnected from the bus until the core is fully set up. */
	OTG_FS_DCTL |= OTG_DCTL_SDIS;

	/*
	 * Power up the PHY. VBUS sensing is turned off and the B-session valid
	 * signal forced instead, so the device enumerates whether or not
	 * PA9 (OTG_FS_VBUS) is wired to the connector.
	 */
	OTG_FS_GCCFG &= ~(OTG_GCCFG_VBDEN | OTG_GCCFG_SDEN | OTG_GCCFG_PDEN |
		OTG_GCCFG_DCDEN | OTG_GCCFG_BCDEN);
	OTG_FS_GCCFG |= OTG_GCCFG_PWRDWN;
	OTG_FS_GOTGCTL |= OTG_GOTGCTL_BVALOEN | OTG_GOTGCTL_BVALOVAL;

	/* Force device mode and wait for the core to leave host mode. */
	OTG_FS_GUSBCFG = (OTG_FS_GUSBCFG & ~(OTG_GUSBCFG_FHMOD | OTG_GUSBCFG_TRDT_MASK)) |
		OTG_GUSBCFG_FDMOD | OTG_GUSBCFG_TRDT_AHB_32MHZ;
	while ((OTG_FS_GINTSTS & OTG_GINTSTS_CMOD) != 0U)
		continue;

	/* Full speed on the embedded PHY, device address 0. */
	OTG_FS_DCFG = (OTG_FS_DCFG & ~(OTG_DCFG_DAD | OTG_DCFG_PFIVL)) | OTG_DCFG_DSPD;
	OTG_FS_PCGCCTL = 0U;

	/* Clear anything pending, then unmask the events dwc_poll() handles. */
	OTG_FS_GINTSTS = UINT32_MAX;
	OTG_FS_GINTMSK = OTG_GINTMSK_RXFLVLM | OTG_GINTMSK_USBSUSPM | OTG_GINTMSK_USBRST |
		OTG_GINTMSK_ENUMDNEM | OTG_GINTMSK_IEPINT | OTG_GINTMSK_OEPINT | OTG_GINTMSK_WUIM;
	OTG_FS_DAINTMSK = 0x00ff00ffU;
	OTG_FS_DIEPMSK = OTG_DIEPMSK_XFRCM;
	OTG_FS_DOEPMSK = OTG_DOEPMSK_STUPM | OTG_DOEPMSK_XFRCM;

	/* Global interrupt gate (status bits only; the NVIC line stays off). */
	OTG_FS_GAHBCFG = OTG_GAHBCFG_GINT;

	/* Connect: enable the DP pull-up. */
	OTG_FS_DCTL &= ~OTG_DCTL_SDIS;
	return &usbd_dev;
}
