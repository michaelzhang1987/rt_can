/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-10-30     SummerGift   first version
 */

#include "drv_common.h"
#include "uart_config.h"
#include "board.h"

#ifdef RT_USING_SERIAL

#include <rtdevice.h>

#if (defined(SOC_SERIES_STM32L0) || defined(SOC_SERIES_STM32H7)) && defined(RT_SERIAL_USING_DMA)
#error "The STM32L0 and STM32H7 devices DO NOT support UART DMA feature."
#elif defined(RT_SERIAL_USING_DMA)
#include "dma_config.h"
#endif

//#define DRV_DEBUG
#define DBG_TAG              "drv.usart"
#ifdef DRV_DEBUG
#define DBG_LVL               DBG_LOG
#else
#define DBG_LVL               DBG_INFO
#endif /* DRV_DEBUG */
#include <rtdbg.h>

#if defined(SOC_SERIES_STM32F0) || defined(SOC_SERIES_STM32F1) || defined(SOC_SERIES_STM32L4) \
    || defined(SOC_SERIES_STM32L0) || defined(SOC_SERIES_STM32G0) || defined(SOC_SERIES_STM32G4)
#define DMA_INSTANCE_TYPE              DMA_Channel_TypeDef
#elif defined(SOC_SERIES_STM32F2) || defined(SOC_SERIES_STM32F4) || defined(SOC_SERIES_STM32F7) || defined(SOC_SERIES_STM32H7)
#define DMA_INSTANCE_TYPE              DMA_Stream_TypeDef
#endif /*  defined(SOC_SERIES_STM32F1) || defined(SOC_SERIES_STM32L4) */

#if defined(SOC_SERIES_STM32F1) || defined(SOC_SERIES_STM32L4) || defined(SOC_SERIES_STM32F2) \
    || defined(SOC_SERIES_STM32F4) || defined(SOC_SERIES_STM32L0) || defined(SOC_SERIES_STM32G0) \
    || defined(SOC_SERIES_STM32G4)
#define UART_INSTANCE_CLEAR_FUNCTION    __HAL_UART_CLEAR_FLAG
#elif defined(SOC_SERIES_STM32F7) || defined(SOC_SERIES_STM32F0) || defined(SOC_SERIES_STM32H7)
#define UART_INSTANCE_CLEAR_FUNCTION    __HAL_UART_CLEAR_IT
#endif

#ifdef RT_SERIAL_USING_DMA
/* --------------------------  DMA   config  -------------------------- */
#if defined(SOC_SERIES_STM32F0) || defined(SOC_SERIES_STM32F1) || defined(SOC_SERIES_STM32L0) \
    || defined(SOC_SERIES_STM32L4) || defined(SOC_SERIES_STM32G0) || defined(SOC_SERIES_STM32G4)
#define DMA_INSTANCE_TYPE              DMA_Channel_TypeDef
#elif defined(SOC_SERIES_STM32F2) || defined(SOC_SERIES_STM32F4) || defined(SOC_SERIES_STM32F7)\
    || defined(SOC_SERIES_STM32H7)
#define DMA_INSTANCE_TYPE              DMA_Stream_TypeDef
#endif /*  defined(SOC_SERIES_STM32F1) || defined(SOC_SERIES_STM32L4) */

struct dma_config {
    DMA_INSTANCE_TYPE *Instance;
    rt_uint32_t dma_rcc;
    IRQn_Type dma_irq;

#if defined(SOC_SERIES_STM32F2) || defined(SOC_SERIES_STM32F4) || defined(SOC_SERIES_STM32F7)
    rt_uint32_t channel;
#endif

#if defined(SOC_SERIES_STM32L4) || defined(SOC_SERIES_STM32G0) || defined(SOC_SERIES_STM32G4)
    rt_uint32_t request;
#endif
};
#endif

/* stm32 config class */
struct stm32_uart_config
{
    const char *name;
    USART_TypeDef *Instance;
    IRQn_Type irq_type;

#ifdef RT_SERIAL_USING_DMA
    struct dma_config *dma_rx;
    struct dma_config *dma_tx;
#endif

    GPIO_TypeDef *tx_port;
    GPIO_TypeDef *rx_port;
    rt_uint32_t tx_pin;
    rt_uint32_t rx_pin;
};

/* stm32 uart dirver class */
struct stm32_uart
{
    UART_HandleTypeDef handle;
    struct stm32_uart_config *config;

#ifdef RT_SERIAL_USING_DMA
    struct
    {
        DMA_HandleTypeDef handle;
        rt_size_t last_index;
    } dma_rx;
    struct
    {
        DMA_HandleTypeDef handle;
    } dma_tx;
#endif
    rt_uint16_t uart_dma_flag;
    struct rt_serial_device serial;
};

#if !defined(BSP_USING_UART1) && !defined(BSP_USING_UART2) && !defined(BSP_USING_UART3) && \
    !defined(BSP_USING_UART4) && !defined(BSP_USING_UART5) && !defined(BSP_USING_UART6) && \
    !defined(BSP_USING_UART7) && !defined(BSP_USING_UART8) && !defined(BSP_USING_LPUART1)
    #error "Please define at least one BSP_USING_UARTx"
    /* this driver can be disabled at menuconfig -> RT-Thread Components -> Device Drivers */
#endif

#ifdef RT_SERIAL_USING_DMA
    static void stm32_dma_config(struct rt_serial_device *serial, rt_ubase_t flag);
#endif

enum
{
#ifdef BSP_USING_UART1
    UART1_INDEX,
#endif
#ifdef BSP_USING_UART2
    UART2_INDEX,
#endif
#ifdef BSP_USING_UART3
    UART3_INDEX,
#endif
#ifdef BSP_USING_UART4
    UART4_INDEX,
#endif
#ifdef BSP_USING_UART5
    UART5_INDEX,
#endif
#ifdef BSP_USING_UART6
    UART6_INDEX,
#endif
#ifdef BSP_USING_UART7
    UART7_INDEX,
#endif
#ifdef BSP_USING_UART8
    UART8_INDEX,
#endif
#ifdef BSP_USING_LPUART1
    LPUART1_INDEX,
#endif
};

static struct stm32_uart_config uart_config[] =
{
#ifdef BSP_USING_UART1
    UART1_CONFIG,
#endif
#ifdef BSP_USING_UART2
    UART2_CONFIG,
#endif
#ifdef BSP_USING_UART3
    UART3_CONFIG,
#endif
#ifdef BSP_USING_UART4
    UART4_CONFIG,
#endif
#ifdef BSP_USING_UART5
    UART5_CONFIG,
#endif
#ifdef BSP_USING_UART6
    UART6_CONFIG,
#endif
#ifdef BSP_USING_UART7
    UART7_CONFIG,
#endif
#ifdef BSP_USING_UART8
    UART8_CONFIG,
#endif
#ifdef BSP_USING_LPUART1
    LPUART1_CONFIG,
#endif
};

static rt_err_t stm32_uart_clk_enable(struct stm32_uart_config *config)
{
    /* uart clock enable */
    switch ((uint32_t)config->Instance)
    {
#ifdef BSP_USING_UART1
    case (uint32_t)USART1:
        __HAL_RCC_USART1_CLK_ENABLE();
        break;
#endif /* BSP_USING_UART1 */
#ifdef BSP_USING_UART2
    case (uint32_t)USART2:
        __HAL_RCC_USART2_CLK_ENABLE();
        break;
#endif /* BSP_USING_UART2 */
#ifdef BSP_USING_UART3
    case (uint32_t)USART3:
        __HAL_RCC_USART3_CLK_ENABLE();
        break;
#endif /* BSP_USING_UART3 */
#ifdef BSP_USING_UART4
#if defined(SOC_SERIES_STM32F0) || defined(SOC_SERIES_STM32L0) || \
   defined(SOC_SERIES_STM32G0)
    case (uint32_t)USART4:
        __HAL_RCC_USART4_CLK_ENABLE();
#else
    case (uint32_t)UART4:
        __HAL_RCC_UART4_CLK_ENABLE();
#endif
        break;
#endif /* BSP_USING_UART4 */
#ifdef BSP_USING_UART5
#if defined(SOC_SERIES_STM32F0) || defined(SOC_SERIES_STM32L0) || \
   defined(SOC_SERIES_STM32G0)
    case (uint32_t)USART5:
        __HAL_RCC_USART5_CLK_ENABLE();
#else
    case (uint32_t)UART5:
        __HAL_RCC_UART5_CLK_ENABLE();
#endif
        break;
#endif /* BSP_USING_UART5 */
#ifdef BSP_USING_UART6
    case (uint32_t)USART6:
        __HAL_RCC_USART6_CLK_ENABLE();
        break;
#endif /* BSP_USING_UART6 */
#ifdef BSP_USING_UART7
#if defined(SOC_SERIES_STM32F0)
    case (uint32_t)USART7:
        __HAL_RCC_USART7_CLK_ENABLE();
#else
    case (uint32_t)UART7:
        __HAL_RCC_UART7_CLK_ENABLE();
#endif
        break;
#endif /* BSP_USING_UART7 */
#ifdef BSP_USING_UART8
#if defined(SOC_SERIES_STM32F0)
    case (uint32_t)USART8:
        __HAL_RCC_USART8_CLK_ENABLE();
#else
    case (uint32_t)UART8:
        __HAL_RCC_UART8_CLK_ENABLE();
#endif
        break;
#endif /* BSP_USING_UART8 */
#ifdef BSP_USING_LPUART1
    case (uint32_t)LPUART1:
       __HAL_RCC_LPUART1_CLK_ENABLE();
        break;
#endif /* BSP_USING_LPUART1 */
    default:
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t stm32_gpio_clk_enable(GPIO_TypeDef *gpiox)
{
    /* check the parameters */
    RT_ASSERT(IS_GPIO_ALL_INSTANCE(gpiox));

    /* gpio ports clock enable */
    switch ((uint32_t)gpiox)
    {
#if defined(__HAL_RCC_GPIOA_CLK_ENABLE)
    case (uint32_t)GPIOA:
        __HAL_RCC_GPIOA_CLK_ENABLE();
        break;
#endif
#if defined(__HAL_RCC_GPIOB_CLK_ENABLE)
    case (uint32_t)GPIOB:
        __HAL_RCC_GPIOB_CLK_ENABLE();
        break;
#endif
#if defined(__HAL_RCC_GPIOC_CLK_ENABLE)
    case (uint32_t)GPIOC:
        __HAL_RCC_GPIOC_CLK_ENABLE();
        break;
#endif
#if defined(__HAL_RCC_GPIOD_CLK_ENABLE)
    case (uint32_t)GPIOD:
        __HAL_RCC_GPIOD_CLK_ENABLE();
        break;
#endif
#if defined(__HAL_RCC_GPIOE_CLK_ENABLE)
    case (uint32_t)GPIOE:
        __HAL_RCC_GPIOE_CLK_ENABLE();
        break;
#endif
#if defined(__HAL_RCC_GPIOF_CLK_ENABLE)
    case (uint32_t)GPIOF:
        __HAL_RCC_GPIOF_CLK_ENABLE();
        break;
#endif
#if defined(__HAL_RCC_GPIOG_CLK_ENABLE)
    case (uint32_t)GPIOG:
        __HAL_RCC_GPIOG_CLK_ENABLE();
        break;
#endif
#if defined(__HAL_RCC_GPIOH_CLK_ENABLE)
    case (uint32_t)GPIOH:
        __HAL_RCC_GPIOH_CLK_ENABLE();
        break;
#endif
#if defined(__HAL_RCC_GPIOI_CLK_ENABLE)
    case (uint32_t)GPIOI:
        __HAL_RCC_GPIOI_CLK_ENABLE();
        break;
#endif
#if defined(__HAL_RCC_GPIOJ_CLK_ENABLE)
    case (uint32_t)GPIOJ:
        __HAL_RCC_GPIOJ_CLK_ENABLE();
        break;
#endif
#if defined(__HAL_RCC_GPIOK_CLK_ENABLE)
    case (uint32_t)GPIOK:
        __HAL_RCC_GPIOK_CLK_ENABLE();
        break;
#endif
    default:
        return -RT_ERROR;
    }

    return RT_EOK;
}

static uint16_t stm32_get_pin(GPIO_TypeDef *pin_port, rt_uint32_t pin_num)
{
    return (uint16_t)((16 * (((rt_base_t)pin_port - (rt_base_t)GPIOA_BASE)/(0x0400UL))) + (__rt_ffs(pin_num) - 1));
}

static rt_err_t stm32_gpio_configure(struct stm32_uart_config *config)
{
    rt_uint16_t tx_pin_num = 0, rx_pin_num = 0;
    int rx_index = 0, tx_index = 0, index = 0;
    int uart_num = 0;
    rt_bool_t uart_is_remap = 0;

    struct gpio_uart_remap {
        /* index get by GET_PIN */
        rt_uint16_t pin_index;
        /* -1: not uart, 1: uart1, 2: uart2 ... */
        rt_int8_t normal_uart;
        /* -1: not uart, 1: uart1, 2: uart2 ... */
        rt_int8_t remap_uart;
    };

    static const struct gpio_uart_remap uart_remaps[] =
    {
        /* usart1 configure */
        { .pin_index = GET_PIN(A, 9),  .normal_uart =  1, .remap_uart = -1 },
        { .pin_index = GET_PIN(A, 10), .normal_uart =  1, .remap_uart = -1 },
        { .pin_index = GET_PIN(B, 6),  .normal_uart = -1, .remap_uart =  1 },
        { .pin_index = GET_PIN(B, 7),  .normal_uart = -1, .remap_uart =  1 },

        /* usart2 configure */
        { .pin_index = GET_PIN(A, 2),  .normal_uart =  2, .remap_uart = -1 },
        { .pin_index = GET_PIN(A, 3),  .normal_uart =  2, .remap_uart = -1 },
        { .pin_index = GET_PIN(D, 5),  .normal_uart = -1, .remap_uart =  2 },
        { .pin_index = GET_PIN(D, 6),  .normal_uart = -1, .remap_uart =  2 },

        /* usart3 configure */
        { .pin_index = GET_PIN(B, 10), .normal_uart =  3, .remap_uart = -1 },
        { .pin_index = GET_PIN(B, 11), .normal_uart =  3, .remap_uart = -1 },
        { .pin_index = GET_PIN(D, 8),  .normal_uart = -1, .remap_uart =  3 },
        { .pin_index = GET_PIN(D, 9),  .normal_uart = -1, .remap_uart =  3 },
        { .pin_index = GET_PIN(C, 10), .normal_uart =  4, .remap_uart =  3 },
        { .pin_index = GET_PIN(C, 11), .normal_uart =  4, .remap_uart =  3 },
    };

    /* get tx/rx pin index */
    tx_pin_num = stm32_get_pin(config->tx_port, config->tx_pin);
    rx_pin_num = stm32_get_pin(config->rx_port, config->rx_pin);

    for (index = 0; index < sizeof(uart_remaps) / sizeof(struct gpio_uart_remap); index++)
    {
        if (uart_remaps[index].pin_index == tx_pin_num)
        {
            tx_index = index;
        }
        else if (uart_remaps[index].pin_index == rx_pin_num)
        {
            rx_index = index;
        }
    }

    /* check tx/rx pin remap information */
    RT_ASSERT(uart_remaps[tx_index].normal_uart == uart_remaps[rx_index].normal_uart);
    RT_ASSERT(uart_remaps[tx_index].remap_uart == uart_remaps[rx_index].remap_uart);

    uart_num = config->name[4] - '0';
    uart_is_remap = uart_remaps[tx_index].remap_uart == uart_num ? RT_TRUE : RT_FALSE;

    {

        GPIO_InitTypeDef GPIO_InitStruct = {0};

        /* gpio ports clock enable */
        stm32_gpio_clk_enable(config->tx_port);
        if (config->tx_port != config->rx_port)
        {
            stm32_gpio_clk_enable(config->rx_port);
        }

        /* tx pin initialize */
        GPIO_InitStruct.Pin = config->tx_pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(config->tx_port, &GPIO_InitStruct);

        /* rx pin initialize */
        GPIO_InitStruct.Pin = config->rx_pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(config->rx_port, &GPIO_InitStruct);

        /* enable the remapping of usart alternate */
        if (uart_is_remap)
        {
            __HAL_RCC_AFIO_CLK_ENABLE();
            switch (uart_num)
            {
            case 1:
                __HAL_AFIO_REMAP_USART1_ENABLE();
                break;
            case 2:
                __HAL_AFIO_REMAP_USART2_ENABLE();
                break;
            case 3:
                if (uart_remaps[tx_index].normal_uart < 0)
                    __HAL_AFIO_REMAP_USART3_ENABLE();
                else
                    __HAL_AFIO_REMAP_USART3_PARTIAL();
                break;
            default:
                RT_ASSERT(0);
            }
        }
    }

    return RT_EOK;
}

static struct stm32_uart uart_obj[sizeof(uart_config) / sizeof(uart_config[0])] = {0};

static rt_err_t stm32_configure(struct rt_serial_device *serial, struct serial_configure *cfg)
{
    struct stm32_uart *uart;
    RT_ASSERT(serial != RT_NULL);
    RT_ASSERT(cfg != RT_NULL);

    uart = rt_container_of(serial, struct stm32_uart, serial);

    /* uart clock enable */
    stm32_uart_clk_enable(uart->config);
    /* uart gpio clock enable and gpio pin init */
    stm32_gpio_configure(uart->config);

    uart->handle.Instance          = uart->config->Instance;
    uart->handle.Init.BaudRate     = cfg->baud_rate;
    uart->handle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    uart->handle.Init.Mode         = UART_MODE_TX_RX;
    uart->handle.Init.OverSampling = UART_OVERSAMPLING_16;
    switch (cfg->data_bits)
    {
    case DATA_BITS_8:
        uart->handle.Init.WordLength = UART_WORDLENGTH_8B;
        break;
    case DATA_BITS_9:
        uart->handle.Init.WordLength = UART_WORDLENGTH_9B;
        break;
    default:
        uart->handle.Init.WordLength = UART_WORDLENGTH_8B;
        break;
    }
    switch (cfg->stop_bits)
    {
    case STOP_BITS_1:
        uart->handle.Init.StopBits   = UART_STOPBITS_1;
        break;
    case STOP_BITS_2:
        uart->handle.Init.StopBits   = UART_STOPBITS_2;
        break;
    default:
        uart->handle.Init.StopBits   = UART_STOPBITS_1;
        break;
    }
    switch (cfg->parity)
    {
    case PARITY_NONE:
        uart->handle.Init.Parity     = UART_PARITY_NONE;
        break;
    case PARITY_ODD:
        uart->handle.Init.Parity     = UART_PARITY_ODD;
        break;
    case PARITY_EVEN:
        uart->handle.Init.Parity     = UART_PARITY_EVEN;
        break;
    default:
        uart->handle.Init.Parity     = UART_PARITY_NONE;
        break;
    }

    if (HAL_UART_Init(&uart->handle) != HAL_OK)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t stm32_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    struct stm32_uart *uart;
#ifdef RT_SERIAL_USING_DMA
    rt_ubase_t ctrl_arg = (rt_ubase_t)arg;
#endif

    RT_ASSERT(serial != RT_NULL);
    uart = rt_container_of(serial, struct stm32_uart, serial);

    switch (cmd)
    {
    /* disable interrupt */
    case RT_DEVICE_CTRL_CLR_INT:
        /* disable rx irq */
        NVIC_DisableIRQ(uart->config->irq_type);
        /* disable interrupt */
        __HAL_UART_DISABLE_IT(&(uart->handle), UART_IT_RXNE);
        break;
    /* enable interrupt */
    case RT_DEVICE_CTRL_SET_INT:
        /* enable rx irq */
        NVIC_EnableIRQ(uart->config->irq_type);
        /* enable interrupt */
        __HAL_UART_ENABLE_IT(&(uart->handle), UART_IT_RXNE);
        break;

#ifdef RT_SERIAL_USING_DMA
    case RT_DEVICE_CTRL_CONFIG:
        stm32_dma_config(serial, ctrl_arg);
        break;
#endif
    }
    return RT_EOK;
}

static int stm32_putc(struct rt_serial_device *serial, char c)
{
    struct stm32_uart *uart;
    RT_ASSERT(serial != RT_NULL);

    uart = rt_container_of(serial, struct stm32_uart, serial);
    UART_INSTANCE_CLEAR_FUNCTION(&(uart->handle), UART_FLAG_TC);
#if defined(SOC_SERIES_STM32L4) || defined(SOC_SERIES_STM32F7) || defined(SOC_SERIES_STM32F0) \
    || defined(SOC_SERIES_STM32L0) || defined(SOC_SERIES_STM32G0) || defined(SOC_SERIES_STM32H7) \
    || defined(SOC_SERIES_STM32G4)
    uart->handle.Instance->TDR = c;
#else
    uart->handle.Instance->DR = c;
#endif
    while (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_TC) == RESET);
    return 1;
}

static int stm32_getc(struct rt_serial_device *serial)
{
    int ch;
    struct stm32_uart *uart;
    RT_ASSERT(serial != RT_NULL);
    uart = rt_container_of(serial, struct stm32_uart, serial);

    ch = -1;
    if (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_RXNE) != RESET)
    {
#if defined(SOC_SERIES_STM32L4) || defined(SOC_SERIES_STM32F7) || defined(SOC_SERIES_STM32F0) \
    || defined(SOC_SERIES_STM32L0) || defined(SOC_SERIES_STM32G0) || defined(SOC_SERIES_STM32H7) \
    || defined(SOC_SERIES_STM32G4)
        ch = uart->handle.Instance->RDR & 0xff;
#else
        ch = uart->handle.Instance->DR & 0xff;
#endif
    }
    return ch;
}

static rt_size_t stm32_dma_transmit(struct rt_serial_device *serial, rt_uint8_t *buf, rt_size_t size, int direction)
{
    struct stm32_uart *uart;
    RT_ASSERT(serial != RT_NULL);
    uart = rt_container_of(serial, struct stm32_uart, serial);

    if (size == 0)
    {
        return 0;
    }

    if (RT_SERIAL_DMA_TX == direction)
    {
        if (HAL_UART_Transmit_DMA(&uart->handle, buf, size) == HAL_OK)
        {
            return size;
        }
        else
        {
            return 0;
        }
    }
    return 0;
}

static const struct rt_uart_ops stm32_uart_ops =
{
    .configure = stm32_configure,
    .control = stm32_control,
    .putc = stm32_putc,
    .getc = stm32_getc,
    .dma_transmit = stm32_dma_transmit
};

/**
 * Uart common interrupt process. This need add to uart ISR.
 *
 * @param serial serial device
 */
static void uart_isr(struct rt_serial_device *serial)
{
    struct stm32_uart *uart;
#ifdef RT_SERIAL_USING_DMA
    rt_size_t recv_total_index, recv_len;
    rt_base_t level;
#endif

    RT_ASSERT(serial != RT_NULL);
    uart = rt_container_of(serial, struct stm32_uart, serial);

    /* UART in mode Receiver -------------------------------------------------*/
    if ((__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_RXNE) != RESET) &&
            (__HAL_UART_GET_IT_SOURCE(&(uart->handle), UART_IT_RXNE) != RESET))
    {
        rt_hw_serial_isr(serial, RT_SERIAL_EVENT_RX_IND);
    }
#ifdef RT_SERIAL_USING_DMA
    else if ((uart->uart_dma_flag) && (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_IDLE) != RESET)
             && (__HAL_UART_GET_IT_SOURCE(&(uart->handle), UART_IT_IDLE) != RESET))
    {
        level = rt_hw_interrupt_disable();
        recv_total_index = serial->config.bufsz - __HAL_DMA_GET_COUNTER(&(uart->dma_rx.handle));
        recv_len = recv_total_index - uart->dma_rx.last_index;
        uart->dma_rx.last_index = recv_total_index;
        rt_hw_interrupt_enable(level);

        if (recv_len)
        {
            rt_hw_serial_isr(serial, RT_SERIAL_EVENT_RX_DMADONE | (recv_len << 8));
        }
        __HAL_UART_CLEAR_IDLEFLAG(&uart->handle);
    }
    else if (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_TC) != RESET)
    {
        if ((serial->parent.open_flag & RT_DEVICE_FLAG_DMA_TX) != 0)
        {
            HAL_UART_IRQHandler(&(uart->handle));
        }
        else
        {
            UART_INSTANCE_CLEAR_FUNCTION(&(uart->handle), UART_FLAG_TC);
        }
    }
#endif
    else
    {
        if (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_ORE) != RESET)
        {
            __HAL_UART_CLEAR_OREFLAG(&uart->handle);
        }
        if (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_NE) != RESET)
        {
            __HAL_UART_CLEAR_NEFLAG(&uart->handle);
        }
        if (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_FE) != RESET)
        {
            __HAL_UART_CLEAR_FEFLAG(&uart->handle);
        }
        if (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_PE) != RESET)
        {
            __HAL_UART_CLEAR_PEFLAG(&uart->handle);
        }
#if !defined(SOC_SERIES_STM32L4) && !defined(SOC_SERIES_STM32F7) && !defined(SOC_SERIES_STM32F0) \
    && !defined(SOC_SERIES_STM32L0) && !defined(SOC_SERIES_STM32G0) && !defined(SOC_SERIES_STM32H7) \
    && !defined(SOC_SERIES_STM32G4)
        if (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_LBD) != RESET)
        {
            UART_INSTANCE_CLEAR_FUNCTION(&(uart->handle), UART_FLAG_LBD);
        }
#endif
        if (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_CTS) != RESET)
        {
            UART_INSTANCE_CLEAR_FUNCTION(&(uart->handle), UART_FLAG_CTS);
        }
        if (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_TXE) != RESET)
        {
            UART_INSTANCE_CLEAR_FUNCTION(&(uart->handle), UART_FLAG_TXE);
        }
        if (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_TC) != RESET)
        {
            UART_INSTANCE_CLEAR_FUNCTION(&(uart->handle), UART_FLAG_TC);
        }
        if (__HAL_UART_GET_FLAG(&(uart->handle), UART_FLAG_RXNE) != RESET)
        {
            UART_INSTANCE_CLEAR_FUNCTION(&(uart->handle), UART_FLAG_RXNE);
        }
    }
}

#ifdef RT_SERIAL_USING_DMA
static void dma_isr(struct rt_serial_device *serial)
{
    struct stm32_uart *uart;
    rt_size_t recv_total_index, recv_len;
    rt_base_t level;

    RT_ASSERT(serial != RT_NULL);
    uart = rt_container_of(serial, struct stm32_uart, serial);

    if ((__HAL_DMA_GET_IT_SOURCE(&(uart->dma_rx.handle), DMA_IT_TC) != RESET) ||
            (__HAL_DMA_GET_IT_SOURCE(&(uart->dma_rx.handle), DMA_IT_HT) != RESET))
    {
        level = rt_hw_interrupt_disable();
        recv_total_index = serial->config.bufsz - __HAL_DMA_GET_COUNTER(&(uart->dma_rx.handle));
        if (recv_total_index == 0)
        {
            recv_len = serial->config.bufsz - uart->dma_rx.last_index;
        }
        else
        {
            recv_len = recv_total_index - uart->dma_rx.last_index;
        }
        uart->dma_rx.last_index = recv_total_index;
        rt_hw_interrupt_enable(level);

        if (recv_len)
        {
            rt_hw_serial_isr(serial, RT_SERIAL_EVENT_RX_DMADONE | (recv_len << 8));
        }
    }
}
#endif

#if defined(BSP_USING_UART1)
void USART1_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    uart_isr(&(uart_obj[UART1_INDEX].serial));

    /* leave interrupt */
    rt_interrupt_leave();
}
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART1_RX_USING_DMA)
void UART1_DMA_RX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART1_INDEX].dma_rx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_UART1_RX_USING_DMA) */
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART1_TX_USING_DMA)
void UART1_DMA_TX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART1_INDEX].dma_tx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_UART1_TX_USING_DMA) */
#endif /* BSP_USING_UART1 */

#if defined(BSP_USING_UART2)
void USART2_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    uart_isr(&(uart_obj[UART2_INDEX].serial));

    /* leave interrupt */
    rt_interrupt_leave();
}
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART2_RX_USING_DMA)
void UART2_DMA_RX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART2_INDEX].dma_rx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_UART2_RX_USING_DMA) */
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART2_TX_USING_DMA)
void UART2_DMA_TX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART2_INDEX].dma_tx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_UART2_TX_USING_DMA) */
#endif /* BSP_USING_UART2 */

#if defined(BSP_USING_UART3)
void USART3_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    uart_isr(&(uart_obj[UART3_INDEX].serial));

    /* leave interrupt */
    rt_interrupt_leave();
}
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART3_RX_USING_DMA)
void UART3_DMA_RX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART3_INDEX].dma_rx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(BSP_UART_USING_DMA_RX) && defined(BSP_UART3_RX_USING_DMA) */
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART3_TX_USING_DMA)
void UART3_DMA_TX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART3_INDEX].dma_tx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(BSP_UART_USING_DMA_TX) && defined(BSP_UART3_TX_USING_DMA) */
#endif /* BSP_USING_UART3*/

#if defined(BSP_USING_UART4)
void UART4_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    uart_isr(&(uart_obj[UART4_INDEX].serial));

    /* leave interrupt */
    rt_interrupt_leave();
}
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART4_RX_USING_DMA)
void UART4_DMA_RX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART4_INDEX].dma_rx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(BSP_UART_USING_DMA_RX) && defined(BSP_UART4_RX_USING_DMA) */

#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART4_TX_USING_DMA)
void UART4_DMA_TX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART4_INDEX].dma_tx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(BSP_UART_USING_DMA_TX) && defined(BSP_UART4_TX_USING_DMA) */
#endif /* BSP_USING_UART4*/

#if defined(BSP_USING_UART5)
void UART5_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    uart_isr(&(uart_obj[UART5_INDEX].serial));

    /* leave interrupt */
    rt_interrupt_leave();
}
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART5_RX_USING_DMA)
void UART5_DMA_RX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART5_INDEX].dma_rx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_UART5_RX_USING_DMA) */
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART5_TX_USING_DMA)
void UART5_DMA_TX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART5_INDEX].dma_tx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_UART5_TX_USING_DMA) */
#endif /* BSP_USING_UART5*/

#if defined(BSP_USING_UART6)
void USART6_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    uart_isr(&(uart_obj[UART6_INDEX].serial));

    /* leave interrupt */
    rt_interrupt_leave();
}
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART6_RX_USING_DMA)
void UART6_DMA_RX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART6_INDEX].dma_rx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_UART6_RX_USING_DMA) */
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART6_TX_USING_DMA)
void UART6_DMA_TX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART6_INDEX].dma_tx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_UART6_TX_USING_DMA) */
#endif /* BSP_USING_UART6*/

#if defined(BSP_USING_UART7)
void UART7_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    uart_isr(&(uart_obj[UART7_INDEX].serial));

    /* leave interrupt */
    rt_interrupt_leave();
}
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART7_RX_USING_DMA)
void UART7_DMA_RX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART7_INDEX].dma_rx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_UART7_RX_USING_DMA) */
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART7_TX_USING_DMA)
void UART7_DMA_TX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART7_INDEX].dma_tx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_UART7_TX_USING_DMA) */
#endif /* BSP_USING_UART7*/

#if defined(BSP_USING_UART8)
void UART8_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    uart_isr(&(uart_obj[UART8_INDEX].serial));

    /* leave interrupt */
    rt_interrupt_leave();
}
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART8_RX_USING_DMA)
void UART8_DMA_RX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART8_INDEX].dma_rx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_UART8_RX_USING_DMA) */
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_UART8_TX_USING_DMA)
void UART8_DMA_TX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[UART8_INDEX].dma_tx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_UART8_TX_USING_DMA) */
#endif /* BSP_USING_UART8*/

#if defined(BSP_USING_LPUART1)
void LPUART1_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    uart_isr(&(uart_obj[LPUART1_INDEX].serial));

    /* leave interrupt */
    rt_interrupt_leave();
}
#if defined(RT_SERIAL_USING_DMA) && defined(BSP_LPUART1_RX_USING_DMA)
void LPUART1_DMA_RX_IRQHandler(void)
{
    /* enter interrupt */
    rt_interrupt_enter();

    HAL_DMA_IRQHandler(&uart_obj[LPUART1_INDEX].dma_rx.handle);

    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* defined(RT_SERIAL_USING_DMA) && defined(BSP_LPUART1_RX_USING_DMA) */
#endif /* BSP_USING_LPUART1*/

#ifdef RT_SERIAL_USING_DMA
static void stm32_dma_config(struct rt_serial_device *serial, rt_ubase_t flag)
{
    struct rt_serial_rx_fifo *rx_fifo;
    DMA_HandleTypeDef *DMA_Handle;
    struct dma_config *dma_config;
    struct stm32_uart *uart;

    RT_ASSERT(serial != RT_NULL);
    uart = rt_container_of(serial, struct stm32_uart, serial);

    if (RT_DEVICE_FLAG_DMA_RX == flag)
    {
        DMA_Handle = &uart->dma_rx.handle;
        dma_config = uart->config->dma_rx;
    }
    else if (RT_DEVICE_FLAG_DMA_TX == flag)
    {
        DMA_Handle = &uart->dma_tx.handle;
        dma_config = uart->config->dma_tx;
    }
    LOG_D("%s dma config start", uart->config->name);

    {
        rt_uint32_t tmpreg = 0x00U;
#if defined(SOC_SERIES_STM32F1) || defined(SOC_SERIES_STM32F0) || defined(SOC_SERIES_STM32G0) \
    || defined(SOC_SERIES_STM32L0)
        /* enable DMA clock && Delay after an RCC peripheral clock enabling*/
        SET_BIT(RCC->AHBENR, dma_config->dma_rcc);
        tmpreg = READ_BIT(RCC->AHBENR, dma_config->dma_rcc);
#elif defined(SOC_SERIES_STM32F4) || defined(SOC_SERIES_STM32F7) || defined(SOC_SERIES_STM32L4) \
    || defined(SOC_SERIES_STM32G4)
        /* enable DMA clock && Delay after an RCC peripheral clock enabling*/
        SET_BIT(RCC->AHB1ENR, dma_config->dma_rcc);
        tmpreg = READ_BIT(RCC->AHB1ENR, dma_config->dma_rcc);

#if (defined(SOC_SERIES_STM32L4) || defined(SOC_SERIES_STM32G4)) && defined(DMAMUX1)
        /* enable DMAMUX clock for L4+ and G4 */
        __HAL_RCC_DMAMUX1_CLK_ENABLE();
#endif

#endif
        UNUSED(tmpreg);   /* To avoid compiler warnings */
    }

    if (RT_DEVICE_FLAG_DMA_RX == flag)
    {
        __HAL_LINKDMA(&(uart->handle), hdmarx, uart->dma_rx.handle);
    }
    else if (RT_DEVICE_FLAG_DMA_TX == flag)
    {
        __HAL_LINKDMA(&(uart->handle), hdmatx, uart->dma_tx.handle);
    }

#if defined(SOC_SERIES_STM32F1) || defined(SOC_SERIES_STM32F0) || defined(SOC_SERIES_STM32L0)
    DMA_Handle->Instance                 = dma_config->Instance;
#elif defined(SOC_SERIES_STM32F4) || defined(SOC_SERIES_STM32F7)
    DMA_Handle->Instance                 = dma_config->Instance;
    DMA_Handle->Init.Channel             = dma_config->channel;
#elif defined(SOC_SERIES_STM32L4) || defined(SOC_SERIES_STM32G0) || defined(SOC_SERIES_STM32G4)
    DMA_Handle->Instance                 = dma_config->Instance;
    DMA_Handle->Init.Request             = dma_config->request;
#endif
    DMA_Handle->Init.PeriphInc           = DMA_PINC_DISABLE;
    DMA_Handle->Init.MemInc              = DMA_MINC_ENABLE;
    DMA_Handle->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    DMA_Handle->Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;

    if (RT_DEVICE_FLAG_DMA_RX == flag)
    {
        DMA_Handle->Init.Direction           = DMA_PERIPH_TO_MEMORY;
        DMA_Handle->Init.Mode                = DMA_CIRCULAR;
    }
    else if (RT_DEVICE_FLAG_DMA_TX == flag)
    {
        DMA_Handle->Init.Direction           = DMA_MEMORY_TO_PERIPH;
        DMA_Handle->Init.Mode                = DMA_NORMAL;
    }

    DMA_Handle->Init.Priority            = DMA_PRIORITY_MEDIUM;
#if defined(SOC_SERIES_STM32F4) || defined(SOC_SERIES_STM32F7)
    DMA_Handle->Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
#endif
    if (HAL_DMA_DeInit(DMA_Handle) != HAL_OK)
    {
        RT_ASSERT(0);
    }

    if (HAL_DMA_Init(DMA_Handle) != HAL_OK)
    {
        RT_ASSERT(0);
    }

    /* enable interrupt */
    if (flag == RT_DEVICE_FLAG_DMA_RX)
    {
        rx_fifo = (struct rt_serial_rx_fifo *)serial->serial_rx;
        /* Start DMA transfer */
        if (HAL_UART_Receive_DMA(&(uart->handle), rx_fifo->buffer, serial->config.bufsz) != HAL_OK)
        {
            /* Transfer error in reception process */
            RT_ASSERT(0);
        }
        CLEAR_BIT(uart->handle.Instance->CR3, USART_CR3_EIE);
        __HAL_UART_ENABLE_IT(&(uart->handle), UART_IT_IDLE);
    }

    /* enable irq */
    HAL_NVIC_SetPriority(dma_config->dma_irq, 0, 0);
    HAL_NVIC_EnableIRQ(dma_config->dma_irq);

    HAL_NVIC_SetPriority(uart->config->irq_type, 1, 0);
    HAL_NVIC_EnableIRQ(uart->config->irq_type);

    LOG_D("%s dma %s instance: %x", uart->config->name, flag == RT_DEVICE_FLAG_DMA_RX ? "RX" : "TX", DMA_Handle->Instance);
    LOG_D("%s dma config done", uart->config->name);
}

/**
  * @brief  UART error callbacks
  * @param  huart: UART handle
  * @note   This example shows a simple way to report transfer error, and you can
  *         add your own implementation.
  * @retval None
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    RT_ASSERT(huart != NULL);
    struct stm32_uart *uart = (struct stm32_uart *)huart;
    LOG_D("%s: %s %d\n", __FUNCTION__, uart->config->name, huart->ErrorCode);
    UNUSED(uart);
}

/**
  * @brief  Rx Transfer completed callback
  * @param  huart: UART handle
  * @note   This example shows a simple way to report end of DMA Rx transfer, and
  *         you can add your own implementation.
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    struct stm32_uart *uart;
    RT_ASSERT(huart != NULL);
    uart = (struct stm32_uart *)huart;
    dma_isr(&uart->serial);
}

/**
  * @brief  Rx Half transfer completed callback
  * @param  huart: UART handle
  * @note   This example shows a simple way to report end of DMA Rx Half transfer,
  *         and you can add your own implementation.
  * @retval None
  */
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    struct stm32_uart *uart;
    RT_ASSERT(huart != NULL);
    uart = (struct stm32_uart *)huart;
    dma_isr(&uart->serial);
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    struct stm32_uart *uart;
    RT_ASSERT(huart != NULL);
    uart = (struct stm32_uart *)huart;
    rt_hw_serial_isr(&uart->serial, RT_SERIAL_EVENT_TX_DMADONE);
}
#endif  /* RT_SERIAL_USING_DMA */

static void stm32_uart_get_dma_config(void)
{
#ifdef BSP_USING_UART1
    uart_obj[UART1_INDEX].uart_dma_flag = 0;
#ifdef BSP_UART1_RX_USING_DMA
    uart_obj[UART1_INDEX].uart_dma_flag |= RT_DEVICE_FLAG_DMA_RX;
    static struct dma_config uart1_dma_rx = UART1_DMA_RX_CONFIG;
    uart_config[UART1_INDEX].dma_rx = &uart1_dma_rx;
#endif
#ifdef BSP_UART1_TX_USING_DMA
    uart_obj[UART1_INDEX].uart_dma_flag |= RT_DEVICE_FLAG_DMA_TX;
    static struct dma_config uart1_dma_tx = UART1_DMA_TX_CONFIG;
    uart_config[UART1_INDEX].dma_tx = &uart1_dma_tx;
#endif
#endif

#ifdef BSP_USING_UART2
    uart_obj[UART2_INDEX].uart_dma_flag = 0;
#ifdef BSP_UART2_RX_USING_DMA
    uart_obj[UART2_INDEX].uart_dma_flag |= RT_DEVICE_FLAG_DMA_RX;
    static struct dma_config uart2_dma_rx = UART2_DMA_RX_CONFIG;
    uart_config[UART2_INDEX].dma_rx = &uart2_dma_rx;
#endif
#ifdef BSP_UART2_TX_USING_DMA
    uart_obj[UART2_INDEX].uart_dma_flag |= RT_DEVICE_FLAG_DMA_TX;
    static struct dma_config uart2_dma_tx = UART2_DMA_TX_CONFIG;
    uart_config[UART2_INDEX].dma_tx = &uart2_dma_tx;
#endif
#endif

#ifdef BSP_USING_UART3
    uart_obj[UART3_INDEX].uart_dma_flag = 0;
#ifdef BSP_UART3_RX_USING_DMA
    uart_obj[UART3_INDEX].uart_dma_flag |= RT_DEVICE_FLAG_DMA_RX;
    static struct dma_config uart3_dma_rx = UART3_DMA_RX_CONFIG;
    uart_config[UART3_INDEX].dma_rx = &uart3_dma_rx;
#endif
#ifdef BSP_UART3_TX_USING_DMA
    uart_obj[UART3_INDEX].uart_dma_flag |= RT_DEVICE_FLAG_DMA_TX;
    static struct dma_config uart3_dma_tx = UART3_DMA_TX_CONFIG;
    uart_config[UART3_INDEX].dma_tx = &uart3_dma_tx;
#endif
#endif

#ifdef BSP_USING_UART4
    uart_obj[UART4_INDEX].uart_dma_flag = 0;
#ifdef BSP_UART4_RX_USING_DMA
    uart_obj[UART4_INDEX].uart_dma_flag |= RT_DEVICE_FLAG_DMA_RX;
    static struct dma_config uart4_dma_rx = UART4_DMA_RX_CONFIG;
    uart_config[UART4_INDEX].dma_rx = &uart4_dma_rx;
#endif
#ifdef BSP_UART4_TX_USING_DMA
    uart_obj[UART4_INDEX].uart_dma_flag |= RT_DEVICE_FLAG_DMA_TX;
    static struct dma_config uart4_dma_tx = UART4_DMA_TX_CONFIG;
    uart_config[UART4_INDEX].dma_tx = &uart4_dma_tx;
#endif
#endif

#ifdef BSP_USING_UART5
    uart_obj[UART5_INDEX].uart_dma_flag = 0;
#ifdef BSP_UART5_RX_USING_DMA
    uart_obj[UART5_INDEX].uart_dma_flag |= RT_DEVICE_FLAG_DMA_RX;
    static struct dma_config uart5_dma_rx = UART5_DMA_RX_CONFIG;
    uart_config[UART5_INDEX].dma_rx = &uart5_dma_rx;
#endif
#ifdef BSP_UART5_TX_USING_DMA
    uart_obj[UART5_INDEX].uart_dma_flag |= RT_DEVICE_FLAG_DMA_TX;
    static struct dma_config uart5_dma_tx = UART5_DMA_TX_CONFIG;
    uart_config[UART5_INDEX].dma_tx = &uart5_dma_tx;
#endif
#endif

#ifdef BSP_USING_UART6
    uart_obj[UART6_INDEX].uart_dma_flag = 0;
#ifdef BSP_UART6_RX_USING_DMA
    uart_obj[UART6_INDEX].uart_dma_flag |= RT_DEVICE_FLAG_DMA_RX;
    static struct dma_config uart6_dma_rx = UART6_DMA_RX_CONFIG;
    uart_config[UART6_INDEX].dma_rx = &uart6_dma_rx;
#endif
#ifdef BSP_UART6_TX_USING_DMA
    uart_obj[UART6_INDEX].uart_dma_flag |= RT_DEVICE_FLAG_DMA_TX;
    static struct dma_config uart6_dma_tx = UART6_DMA_TX_CONFIG;
    uart_config[UART6_INDEX].dma_tx = &uart6_dma_tx;
#endif
#endif
}

int rt_hw_usart_init(void)
{
    rt_size_t obj_num = sizeof(uart_obj) / sizeof(struct stm32_uart);
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    rt_err_t result = 0;

    stm32_uart_get_dma_config();

    for (int i = 0; i < obj_num; i++)
    {
        uart_obj[i].config = &uart_config[i];
        uart_obj[i].serial.ops    = &stm32_uart_ops;
        uart_obj[i].serial.config = config;
        /* register UART device */
        result = rt_hw_serial_register(&uart_obj[i].serial, uart_obj[i].config->name,
                                       RT_DEVICE_FLAG_RDWR
                                       | RT_DEVICE_FLAG_INT_RX
                                       | RT_DEVICE_FLAG_INT_TX
                                       | uart_obj[i].uart_dma_flag
                                       , NULL);
        RT_ASSERT(result == RT_EOK);
    }

    return result;
}

#endif /* RT_USING_SERIAL */
