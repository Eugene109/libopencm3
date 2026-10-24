/** @addtogroup dcmi_file DCMI peripheral API
 * @ingroup peripheral_apis
 * @brief Digital camera interface.
 *
 * The digital camera is a synchronous parallel interface able to
 * receive a high-speed data flow from an external 8-, 10-, 12- or 14-bit
 * CMOS camera module.
 *
 * The DCMI only packs the incoming data into 32-bit words; the image buffer
 * is filled by a DMA stream reading @ref DCMI_DR. A typical setup is:
 *
 * 1. Enable the clock and reset the peripheral with
 *    rcc_periph_clock_enable(RCC_DCMI) and rcc_periph_reset_pulse(RST_DCMI),
 *    then route the DCMI pins to their alternate function.
 * 2. While the DCMI is disabled, set the data width, polarities, capture
 *    mode and the optional crop, JPEG and embedded synchronisation features.
 *    The reference manual requires all configuration to be done before the
 *    interface is enabled.
 * 3. Call dcmi_enable().
 * 4. Configure and enable a DMA stream: peripheral-to-memory, 32-bit
 *    peripheral size, peripheral address &DCMI_DR. On STM32F4/F7 this is
 *    DMA2 stream 1 or 7, channel 1. On STM32H7 any stream of DMA1 or DMA2
 *    can be used, with DMAMUX1 request
 *    DMAMUX1_CxCR_DMAREQ_ID_DCMI_DMA routed to it.
 * 5. Call dcmi_enable_capture(). Capture starts at the next start of frame.
 *
 * If the APIs here are insufficient or incomplete, see @ref dcmi_defines
 *
 * LGPL License Terms @ref lgpl_license
 */
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

/**@{*/

#include <libopencm3/stm32/dcmi.h>

/*---------------------------------------------------------------------------*/
/** @brief Enable the DCMI.
 *
 * All DCMI configuration must be programmed before the interface is enabled.
 */
void dcmi_enable(void)
{
	DCMI_CR |= DCMI_CR_EN;
}

/*---------------------------------------------------------------------------*/
/** @brief Disable the DCMI.
 *
 * Capture is disabled as well. Any frame in progress is abandoned; use
 * dcmi_disable_capture() first and wait for dcmi_is_capture_enabled() to
 * return false to stop at a frame boundary.
 */
void dcmi_disable(void)
{
	DCMI_CR &= ~(DCMI_CR_EN | DCMI_CR_CAPTURE);
}

/*---------------------------------------------------------------------------*/
/** @brief Enable capture.
 *
 * The interface waits for the next start of frame, then issues a DMA request
 * for every 32-bit word received. The DMA stream and the DCMI configuration
 * must be programmed before capture is enabled.
 */
void dcmi_enable_capture(void)
{
	DCMI_CR |= DCMI_CR_CAPTURE;
}

/*---------------------------------------------------------------------------*/
/** @brief Request capture to stop.
 *
 * In continuous mode the hardware finishes the frame in progress before
 * capture actually stops; poll dcmi_is_capture_enabled() to find out when.
 */
void dcmi_disable_capture(void)
{
	DCMI_CR &= ~DCMI_CR_CAPTURE;
}

/*---------------------------------------------------------------------------*/
/** @brief Check whether capture is enabled.
 *
 * The hardware clears the CAPTURE bit at the end of the frame in snapshot
 * mode, and at the end of the current frame after dcmi_disable_capture() in
 * continuous mode.
 *
 * @returns true while capture is enabled
 */
bool dcmi_is_capture_enabled(void)
{
	return (DCMI_CR & DCMI_CR_CAPTURE) != 0;
}

/*---------------------------------------------------------------------------*/
/** @brief Capture a single frame each time capture is enabled.
 */
void dcmi_set_snapshot_mode(void)
{
	DCMI_CR |= DCMI_CR_CM;
}

/*---------------------------------------------------------------------------*/
/** @brief Capture frames continuously until capture is disabled.
 */
void dcmi_set_continuous_mode(void)
{
	DCMI_CR &= ~DCMI_CR_CM;
}

/*---------------------------------------------------------------------------*/
/** @brief Set the number of data bits captured on each pixel clock.
 *
 * In 8-bit mode four pixel clocks make up one 32-bit word. In 10, 12 and
 * 14-bit mode each sample is stored right aligned in 16 bits, so two pixel
 * clocks make up one word. Unused data inputs should be tied to ground.
 *
 * @param[in] width One of @ref DCMI_CR_EDM_8BIT, @ref DCMI_CR_EDM_10BIT,
 * @ref DCMI_CR_EDM_12BIT or @ref DCMI_CR_EDM_14BIT
 */
void dcmi_set_data_width(uint32_t width)
{
	const uint32_t mask = DCMI_CR_EDM_MASK << DCMI_CR_EDM_SHIFT;

	DCMI_CR = (DCMI_CR & ~mask) | (width & mask);
}

/*---------------------------------------------------------------------------*/
/** @brief Set the frame capture rate in continuous mode.
 *
 * This setting is ignored in snapshot mode.
 *
 * @param[in] rate One of @ref DCMI_CR_FCRC_ALL, @ref DCMI_CR_FCRC_1_IN_2 or
 * @ref DCMI_CR_FCRC_1_IN_4
 */
void dcmi_set_frame_capture_rate(uint32_t rate)
{
	const uint32_t mask = DCMI_CR_FCRC_MASK << DCMI_CR_FCRC_SHIFT;

	DCMI_CR = (DCMI_CR & ~mask) | (rate & mask);
}

/*---------------------------------------------------------------------------*/
/** @brief Sample data on the rising edge of DCMI_PIXCLK.
 */
void dcmi_set_pixclk_polarity_rising(void)
{
	DCMI_CR |= DCMI_CR_PCKPOL;
}

/*---------------------------------------------------------------------------*/
/** @brief Sample data on the falling edge of DCMI_PIXCLK (reset default).
 */
void dcmi_set_pixclk_polarity_falling(void)
{
	DCMI_CR &= ~DCMI_CR_PCKPOL;
}

/*---------------------------------------------------------------------------*/
/** @brief DCMI_HSYNC is high while line data is not valid.
 *
 * The HSYNC and VSYNC polarities give the level of the signal during
 * blanking, not while data is valid. For example a sensor that drives HREF
 * high during valid pixels needs dcmi_set_hsync_polarity_low().
 *
 * Both polarities are ignored when embedded synchronisation is used.
 */
void dcmi_set_hsync_polarity_high(void)
{
	DCMI_CR |= DCMI_CR_HSPOL;
}

/*---------------------------------------------------------------------------*/
/** @brief DCMI_HSYNC is low while line data is not valid (reset default).
 *
 * See dcmi_set_hsync_polarity_high().
 */
void dcmi_set_hsync_polarity_low(void)
{
	DCMI_CR &= ~DCMI_CR_HSPOL;
}

/*---------------------------------------------------------------------------*/
/** @brief DCMI_VSYNC is high while frame data is not valid.
 *
 * See dcmi_set_hsync_polarity_high().
 */
void dcmi_set_vsync_polarity_high(void)
{
	DCMI_CR |= DCMI_CR_VSPOL;
}

/*---------------------------------------------------------------------------*/
/** @brief DCMI_VSYNC is low while frame data is not valid (reset default).
 *
 * See dcmi_set_hsync_polarity_high().
 */
void dcmi_set_vsync_polarity_low(void)
{
	DCMI_CR &= ~DCMI_CR_VSPOL;
}

/*---------------------------------------------------------------------------*/
/** @brief Enable JPEG (compressed data) mode.
 *
 * DCMI_VSYNC marks the start and end of the image and DCMI_HSYNC is used as
 * a data valid signal. The crop and embedded synchronisation features cannot
 * be used in this mode. A final partial word is padded with zeros.
 */
void dcmi_enable_jpeg(void)
{
	DCMI_CR |= DCMI_CR_JPEG;
}

/*---------------------------------------------------------------------------*/
/** @brief Disable JPEG mode (uncompressed video).
 */
void dcmi_disable_jpeg(void)
{
	DCMI_CR &= ~DCMI_CR_JPEG;
}

/*---------------------------------------------------------------------------*/
/** @brief Synchronise on codes embedded in the data stream.
 *
 * Only valid with 8-bit data width. The camera must then use 0x00 and 0xFF
 * for synchronisation codes only. Program the codes with
 * dcmi_set_embedded_sync_codes() and dcmi_set_embedded_sync_unmask().
 */
void dcmi_enable_embedded_sync(void)
{
	DCMI_CR |= DCMI_CR_ESS;
}

/*---------------------------------------------------------------------------*/
/** @brief Synchronise on the DCMI_HSYNC and DCMI_VSYNC signals.
 */
void dcmi_disable_embedded_sync(void)
{
	DCMI_CR &= ~DCMI_CR_ESS;
}

/*---------------------------------------------------------------------------*/
/** @brief Set the embedded synchronisation codes.
 *
 * Each code is the last byte of a 0xFF, 0x00, 0x00, code sequence.
 *
 * @param[in] fsc Frame start code. 0xFF means no frame start code is sent and
 * the first line start after a frame end starts the frame.
 * @param[in] lsc Line start code
 * @param[in] lec Line end code
 * @param[in] fec Frame end code. 0xFF means every unused code is taken as a
 * frame end.
 */
void dcmi_set_embedded_sync_codes(uint8_t fsc, uint8_t lsc, uint8_t lec,
				  uint8_t fec)
{
	DCMI_ESCR = ((uint32_t)fec << DCMI_ESCR_FEC_SHIFT) |
		    ((uint32_t)lec << DCMI_ESCR_LEC_SHIFT) |
		    ((uint32_t)lsc << DCMI_ESCR_LSC_SHIFT) |
		    ((uint32_t)fsc << DCMI_ESCR_FSC_SHIFT);
}

/*---------------------------------------------------------------------------*/
/** @brief Set which bits of each embedded code are compared.
 *
 * A bit set to 1 is compared with the matching bit of the code programmed by
 * dcmi_set_embedded_sync_codes(); a bit set to 0 is ignored. The reset value
 * ignores every bit, so this usually needs to be set to 0xFF for all four.
 *
 * @param[in] fsu Frame start code unmask
 * @param[in] lsu Line start code unmask
 * @param[in] leu Line end code unmask
 * @param[in] feu Frame end code unmask
 */
void dcmi_set_embedded_sync_unmask(uint8_t fsu, uint8_t lsu, uint8_t leu,
				   uint8_t feu)
{
	DCMI_ESUR = ((uint32_t)feu << DCMI_ESUR_FEU_SHIFT) |
		    ((uint32_t)leu << DCMI_ESUR_LEU_SHIFT) |
		    ((uint32_t)lsu << DCMI_ESUR_LSU_SHIFT) |
		    ((uint32_t)fsu << DCMI_ESUR_FSU_SHIFT);
}

/*---------------------------------------------------------------------------*/
/** @brief Capture only the window set by dcmi_set_crop_window().
 */
void dcmi_enable_crop(void)
{
	DCMI_CR |= DCMI_CR_CROP;
}

/*---------------------------------------------------------------------------*/
/** @brief Capture the full image.
 *
 * The number of bytes in a frame must then be a multiple of four.
 */
void dcmi_disable_crop(void)
{
	DCMI_CR &= ~DCMI_CR_CROP;
}

/*---------------------------------------------------------------------------*/
/** @brief Set the crop window.
 *
 * Horizontal values are in pixel clocks, not pixels: in 8-bit mode a
 * RGB565 or YCbCr 4:2:2 pixel takes two pixel clocks.
 *
 * @param[in] hoffset Pixel clocks to skip at the start of each line (0-16383)
 * @param[in] vstart First line to capture, counting from 0 (0-8191)
 * @param[in] capture_count Pixel clocks to capture on each line (1-16384).
 * Must make whole 32-bit words: a multiple of 4 in 8-bit mode, a multiple of 2
 * otherwise.
 * @param[in] line_count Lines to capture (1-16384)
 */
void dcmi_set_crop_window(uint16_t hoffset, uint16_t vstart,
			  uint16_t capture_count, uint16_t line_count)
{
	DCMI_CWSTRT = ((uint32_t)(vstart & DCMI_CWSTRT_VST_MASK) <<
		       DCMI_CWSTRT_VST_SHIFT) |
		      ((uint32_t)(hoffset & DCMI_CWSTRT_HOFFCNT_MASK) <<
		       DCMI_CWSTRT_HOFFCNT_SHIFT);
	DCMI_CWSIZE = ((uint32_t)((line_count - 1) & DCMI_CWSIZE_VLINE_MASK) <<
		       DCMI_CWSIZE_VLINE_SHIFT) |
		      ((uint32_t)((capture_count - 1) & DCMI_CWSIZE_CAPCNT_MASK) <<
		       DCMI_CWSIZE_CAPCNT_SHIFT);
}

/*---------------------------------------------------------------------------*/
/** @brief Enable DCMI interrupts.
 *
 * The NVIC interrupt (NVIC_DCMI_IRQ) must also be enabled.
 *
 * @param[in] irq Bitwise OR of @ref DCMI_IER_LINE, @ref DCMI_IER_VSYNC,
 * @ref DCMI_IER_ERR, @ref DCMI_IER_OVR and @ref DCMI_IER_FRAME
 */
void dcmi_enable_irq(uint32_t irq)
{
	DCMI_IER |= irq;
}

/*---------------------------------------------------------------------------*/
/** @brief Disable DCMI interrupts.
 *
 * @param[in] irq Bitwise OR of DCMI_IER_xxx values, see dcmi_enable_irq()
 */
void dcmi_disable_irq(uint32_t irq)
{
	DCMI_IER &= ~irq;
}

/*---------------------------------------------------------------------------*/
/** @brief Read a raw status flag.
 *
 * Flags are set whether or not the matching interrupt is enabled.
 *
 * @param[in] flag One of @ref DCMI_RIS_LINE, @ref DCMI_RIS_VSYNC,
 * @ref DCMI_RIS_ERR, @ref DCMI_RIS_OVR or @ref DCMI_RIS_FRAME
 * @returns true if the flag is set
 */
bool dcmi_get_flag(uint32_t flag)
{
	return (DCMI_RIS & flag) != 0;
}

/*---------------------------------------------------------------------------*/
/** @brief Read a masked interrupt flag.
 *
 * A flag only reads as set if its interrupt is enabled in DCMI_IER.
 *
 * @param[in] flag One of @ref DCMI_MIS_LINE, @ref DCMI_MIS_VSYNC,
 * @ref DCMI_MIS_ERR, @ref DCMI_MIS_OVR or @ref DCMI_MIS_FRAME
 * @returns true if the flag is set
 */
bool dcmi_get_interrupt_flag(uint32_t flag)
{
	return (DCMI_MIS & flag) != 0;
}

/*---------------------------------------------------------------------------*/
/** @brief Clear status flags.
 *
 * @param[in] flag Bitwise OR of @ref DCMI_ICR_LINE, @ref DCMI_ICR_VSYNC,
 * @ref DCMI_ICR_ERR, @ref DCMI_ICR_OVR and @ref DCMI_ICR_FRAME
 */
void dcmi_clear_flag(uint32_t flag)
{
	DCMI_ICR = flag;
}

/*---------------------------------------------------------------------------*/
/** @brief Select which received bytes are captured.
 *
 * Only works with 8-bit data width; for any other width the mode must be
 * left at @ref DCMI_CR_BSM_ALL. Not available on
 * STM32F407/417/427/429/437/439.
 *
 * @param[in] mode One of @ref DCMI_CR_BSM_ALL, @ref DCMI_CR_BSM_1_IN_2,
 * @ref DCMI_CR_BSM_1_IN_4 or @ref DCMI_CR_BSM_2_IN_4, optionally ORed with
 * @ref DCMI_CR_OEBS to start with the second byte after the frame/line start
 * instead of the first.
 */
void dcmi_set_byte_select(uint32_t mode)
{
	const uint32_t mask = (DCMI_CR_BSM_MASK << DCMI_CR_BSM_SHIFT) |
			      DCMI_CR_OEBS;

	DCMI_CR = (DCMI_CR & ~mask) | (mode & mask);
}

/*---------------------------------------------------------------------------*/
/** @brief Select which received lines are captured.
 *
 * Not available on STM32F407/417/427/429/437/439.
 *
 * @param[in] mode 0 to capture every line, @ref DCMI_CR_LSM to capture every
 * other line starting with the first, or DCMI_CR_LSM | @ref DCMI_CR_OELS to
 * capture every other line starting with the second.
 */
void dcmi_set_line_select(uint32_t mode)
{
	const uint32_t mask = DCMI_CR_LSM | DCMI_CR_OELS;

	DCMI_CR = (DCMI_CR & ~mask) | (mode & mask);
}

/**@}*/
