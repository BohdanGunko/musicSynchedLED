#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "ws2812.h"
#include <stdio.h>

#define WS_ZERO_OFFSET      (1)
#define WS_ONE_CODE         (0b110 << 24)
#define WS_ZERO_CODE        (0b100 << 24)
#define WS_SPI_BIT_PER_BIT  (3)
#define WS_COLOR_PER_PIXEL  (3)
#define WS_BYTES_PER_PIXEL  (WS_SPI_BIT_PER_BIT * WS_COLOR_PER_PIXEL)

static uint8_t ws_frame_buffer[WS_ZERO_OFFSET + (WS2812_LEDS_COUNT * WS_BYTES_PER_PIXEL)];
static cyhal_spi_t ws2182_spi_handle;
static uint32_t ws_convert_3_code(uint8_t input);

typedef union
{
    uint8_t bytes[4];
    uint32_t word;
} ws_converted_color_u;

void tx_dma_complete(void)
{
    printf("tx_dma_complete\r\n");
}

#define WS_NUM_DESCRIPTORS (sizeof(ws_frame_buffer) / 256 + 1)
static cy_stc_dma_descriptor_t WSDescriptors[WS_NUM_DESCRIPTORS];
static void WS_DMAConfigure(void)
{
    // I copies this structure from the PSoC Creator Component configuration
    // in generated source
    const cy_stc_dma_descriptor_config_t WS_DMA_Descriptors_config =
    {
    .retrigger       = CY_DMA_RETRIG_IM,
    .interruptType   = CY_DMA_DESCR_CHAIN,
    .triggerOutType  = CY_DMA_1ELEMENT,
    .channelState    = CY_DMA_CHANNEL_ENABLED,
    .triggerInType   = CY_DMA_1ELEMENT,
    .dataSize        = CY_DMA_BYTE,
    .srcTransferSize = CY_DMA_TRANSFER_SIZE_DATA,
    .dstTransferSize = CY_DMA_TRANSFER_SIZE_WORD,
    .descriptorType  = CY_DMA_1D_TRANSFER,
    .srcAddress      = NULL,
    .dstAddress      = NULL,
    .srcXincrement   = 1L,
    .dstXincrement   = 0L,
    .xCount          = 256UL,
    .srcYincrement   = 0L,
    .dstYincrement   = 0L,
    .yCount          = 1UL,
    .nextDescriptor  = 0
    };

    for(unsigned int i=0;i<WS_NUM_DESCRIPTORS;i++)
    {
        Cy_DMA_Descriptor_Init(&WSDescriptors[i], &WS_DMA_Descriptors_config);
        Cy_DMA_Descriptor_SetSrcAddress(&WSDescriptors[i], (uint8_t *)&ws_frame_buffer[i*256]);
        Cy_DMA_Descriptor_SetDstAddress(&WSDescriptors[i], (void *)&ws2182_spi_handle.base->TX_FIFO_WR);
        Cy_DMA_Descriptor_SetXloopDataCount(&WSDescriptors[i],256); // the last
        Cy_DMA_Descriptor_SetNextDescriptor(&WSDescriptors[i],&WSDescriptors[i+1]);
    }

    // The last one needs a bit of change
    Cy_DMA_Descriptor_SetXloopDataCount(&WSDescriptors[WS_NUM_DESCRIPTORS-1],sizeof(ws_frame_buffer)-256*(WS_NUM_DESCRIPTORS-1)); // the last
    Cy_DMA_Descriptor_SetNextDescriptor(&WSDescriptors[WS_NUM_DESCRIPTORS-1],0);
    Cy_DMA_Descriptor_SetChannelState(&WSDescriptors[WS_NUM_DESCRIPTORS-1],CY_DMA_CHANNEL_DISABLED);

    const cy_stc_sysint_t intTxDma_cfg =
     {
         .intrSrc      = WS_DMA_IRQ,
         .intrPriority = 7u
     };

    /* Initialize and enable the interrupt from TxDma */
    Cy_SysInt_Init(&intTxDma_cfg, &tx_dma_complete);
    NVIC_EnableIRQ((IRQn_Type)intTxDma_cfg.intrSrc);

    /* Enable DMA interrupt source. */
    Cy_DMA_Channel_SetInterruptMask(WS_DMA_HW, WS_DMA_CHANNEL, CY_DMA_INTR_MASK);


    Cy_DMA_Enable(WS_DMA_HW);
}

void WS_DMATrigger()
{
    cy_stc_dma_channel_config_t channelConfig;
    channelConfig.descriptor  = &WSDescriptors[0];
    channelConfig.preemptable = false;
    channelConfig.priority    = 3;
    channelConfig.enable      = false;
    Cy_DMA_Channel_Init(WS_DMA_HW, WS_DMA_CHANNEL, &channelConfig);
    Cy_DMA_Channel_Enable(WS_DMA_HW,WS_DMA_CHANNEL);
}

ws2818_res_t ws2812_init(cyhal_gpio_t mosi, cyhal_gpio_t miso, cyhal_gpio_t sclk)
{
    cy_rslt_t cy_res;
    ws2818_res_t ws_res;

    /* Initialize SPI block that will be used to drive data to the LEDs */
    cy_res = cyhal_spi_init(&ws2182_spi_handle, mosi, miso, sclk, NC, NULL, 8, CYHAL_SPI_MODE_11_MSB, false);
    if(CY_RSLT_SUCCESS != cy_res)
    {
        return ws2812_error_generic;
    }

    cy_res = cyhal_spi_set_frequency(&ws2182_spi_handle, 2200000);
    if(CY_RSLT_SUCCESS != cy_res)
    {
        return ws2812_error_generic;
    }

    WS_DMAConfigure();

    /* First byte of transferred data is ignored by WS2812 for some reasons,
     * so it makes sense to zero it out */
    ws_frame_buffer[0] = 0x00;

    /* Turn of all LEDs */
    ws_res =  ws2812_set_all_leds(0, 0, 0);
    if(ws2812_success != ws_res)
    {
        return ws_res;
    }

    ws_res = ws2812_update();
    if(ws2812_success != ws_res)
    {
        return ws_res;
    }

    return ws2812_success;
}

ws2818_res_t ws2812_set_led(uint16_t led, uint8_t red, uint8_t green, uint8_t blue)
{
    if(led > (WS2812_LEDS_COUNT - 1))
    {
        return ws2812_error_invalid_led_id;
    }

    ws_converted_color_u color;

    /* WS2812 expects green then red then blue colours for the LED */
    color.word = ws_convert_3_code(green);
    ws_frame_buffer[WS_ZERO_OFFSET + (led * WS_BYTES_PER_PIXEL) + 0] = color.bytes[2];
    ws_frame_buffer[WS_ZERO_OFFSET + (led * WS_BYTES_PER_PIXEL) + 1] = color.bytes[1];
    ws_frame_buffer[WS_ZERO_OFFSET + (led * WS_BYTES_PER_PIXEL) + 2] = color.bytes[0];

    color.word = ws_convert_3_code(red);
    ws_frame_buffer[WS_ZERO_OFFSET + (led * WS_BYTES_PER_PIXEL) + 3] = color.bytes[2];
    ws_frame_buffer[WS_ZERO_OFFSET + (led * WS_BYTES_PER_PIXEL) + 4] = color.bytes[1];
    ws_frame_buffer[WS_ZERO_OFFSET + (led * WS_BYTES_PER_PIXEL) + 5] = color.bytes[0];

    color.word = ws_convert_3_code(blue);
    ws_frame_buffer[WS_ZERO_OFFSET + (led * WS_BYTES_PER_PIXEL) + 6] = color.bytes[2];
    ws_frame_buffer[WS_ZERO_OFFSET + (led * WS_BYTES_PER_PIXEL) + 7] = color.bytes[1];
    ws_frame_buffer[WS_ZERO_OFFSET + (led * WS_BYTES_PER_PIXEL) + 8] = color.bytes[0];

    return ws2812_success;
}

ws2818_res_t ws2812_set_range(uint16_t start, uint16_t end, uint8_t red, uint8_t green, uint8_t blue)
{
    ws2818_res_t ws_res;
    size_t length = end - start;

    if((start > end) || (end > (WS2812_LEDS_COUNT - 1)))
    {
        return ws2812_error_invalid_led_id;
    }

    /* Set RGB values for the first LED and then copy to the rest of them */
    ws_res = ws2812_set_led(start, red, green, blue);
    if(ws2812_success != ws_res)
    {
        return ws_res;
    }

    for(size_t i = 1; i <= length; i++)
    {
        memcpy(&ws_frame_buffer[(start * WS_BYTES_PER_PIXEL) + (i * WS_BYTES_PER_PIXEL) + WS_ZERO_OFFSET],
               &ws_frame_buffer[(start * WS_BYTES_PER_PIXEL) + WS_ZERO_OFFSET],
               WS_BYTES_PER_PIXEL);
    }

    return ws2812_success;
}

ws2818_res_t ws2812_set_all_leds(uint8_t red, uint8_t green, uint8_t blue)
{
    return ws2812_set_range(0, WS2812_LEDS_COUNT - 1, red, green, blue);
}

/* Send the latest frame buffer to the LEDs */
ws2818_res_t ws2812_update(void)
{
    cy_rslt_t cy_res;

    /* TODO: This may be asynch transfer using Semaphores */
    // cy_res = cyhal_spi_transfer(&ws2182_spi_handle, ws_frame_buffer, WS_ZERO_OFFSET + (WS2812_LEDS_COUNT * WS_BYTES_PER_PIXEL), NULL, 0, 0x00);
    WS_DMATrigger();
    if(CY_RSLT_SUCCESS != cy_res)
    {
        return ws2812_error_generic;
    }

    return ws2812_success;
}

/* This function takes an 8-bit value representing a color
 * and turns it into a WS2812 bit code... where 1=110 and 0=011
 * one input byte turns into three output bytes of a uint32_t
 */
static uint32_t ws_convert_3_code(uint8_t input)
{
    uint32_t ret_val = 0;
    for(size_t i = 0; i < 8; i++)
    {
        if(input % 2)
        {
            ret_val |= WS_ONE_CODE;
        }
        else
        {
            ret_val |= WS_ZERO_CODE;
        }

        ret_val = ret_val >> 3;

        input = input >> 1;
    }

    return ret_val;
}
