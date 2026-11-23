#include "bsp.h"
#include "app_config.h"

UART_HandleTypeDef huart1;   /* 调试备用输出 : PA9  */
TIM_HandleTypeDef  htim2;   /* 电机 4 路 : PA0/PA1/PA2/PA3 */
TIM_HandleTypeDef  htim3;   /* 舵机 3 路 : PA6/PA7/PB0 */
UART_HandleTypeDef huart3;  /* iBUS : PB11 / 调试 TX : PB10 */
ADC_HandleTypeDef  hadc1;   /* 电池 : PA5 */

volatile int32_t g_enc_l;
volatile int32_t g_enc_r;


static inline uint8_t enc_l_state(void);
static inline uint8_t enc_r_state(void);
static uint8_t enc_prev_l, enc_prev_r;

void BSP_GpioInit(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    /* 关键：F1 复位后默认是 JTAG+SWD，PB4 = NJTRST、PB3 = JTDO。
       右编码器 A 相就在 PB4，不关 JTAG 的话 EXTI4 收不到脉冲。 */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    /* PA0-PA3 : TIM2 CH1..CH4 电机 PWM */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    g.Mode = GPIO_MODE_AF_PP; g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    /* PA6 PA7 PB0 : TIM3 舵机 */
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &g);

    /* PA9 : USART1_TX 调试备用输出 */
    g.Mode = GPIO_MODE_AF_PP; g.Speed = GPIO_SPEED_FREQ_HIGH; g.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOA, &g);

    /* 编码器 EXTI：左 PA10/PA11，右 PB4/PB5 */
    g.Mode = GPIO_MODE_IT_RISING_FALLING; g.Pull = GPIO_PULLUP;
    g.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOB, &g);

    /* PA5 电池 ADC */
    g.Mode = GPIO_MODE_ANALOG; g.Pin = GPIO_PIN_5;
    HAL_GPIO_Init(GPIOA, &g);

    /* USART3 */
    g.Mode = GPIO_MODE_AF_PP; g.Speed = GPIO_SPEED_FREQ_HIGH; g.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOB, &g);
    g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP; g.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOB, &g);

    /* 输出：EN_L / EN_R / LED */
    g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = GPIO_PIN_1 | GPIO_PIN_9;
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOC, &g);

    HAL_GPIO_WritePin(EN_L_PORT, EN_L_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(EN_R_PORT, EN_R_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);

    /* 编码器初态：先读一次真实 AB 电平，避免第一次跳变被当成非法跳变丢掉 */
    enc_prev_l = enc_l_state();
    enc_prev_r = enc_r_state();
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_4 | GPIO_PIN_5);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_SetPriority(EXTI4_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
}

/* USART3 中断入口：CubeMX 未生成（NVIC 未在 .ioc 勾选），这里补上，
   否则 RXNE/ORE 一触发就跳到 weak Default_Handler 死循环 */
void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart3);
}

/* 16 项正交解码表：index = (prev<<2)|now，值 = ±1/0 */
static const int8_t quad_tbl[16] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};

static inline uint8_t enc_l_state(void)
{
    uint8_t a = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_SET);
    uint8_t b = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11) == GPIO_PIN_SET);
    return (uint8_t)((a << 1) | b);
}

static inline uint8_t enc_r_state(void)
{
    uint8_t a = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_SET);
    uint8_t b = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_SET);
    return (uint8_t)((a << 1) | b);
}

/* 左编码器：PA10 = A，PA11 = B（EXTI10 / EXTI11） */
void EXTI15_10_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_10) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_10);
        uint8_t s = enc_l_state();
        g_enc_l += quad_tbl[((enc_prev_l << 2) | s) & 0x0F];
        enc_prev_l = s;
    }
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_11) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_11);
        uint8_t s = enc_l_state();
        g_enc_l += quad_tbl[((enc_prev_l << 2) | s) & 0x0F];
        enc_prev_l = s;
    }
}

/* 右编码器：PB4 = A（EXTI4），PB5 = B（EXTI5） */
void EXTI4_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_4) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);
        uint8_t s = enc_r_state();
        g_enc_r += quad_tbl[((enc_prev_r << 2) | s) & 0x0F];
        enc_prev_r = s;
    }
}

void EXTI9_5_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_5) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_5);
        uint8_t s = enc_r_state();
        g_enc_r += quad_tbl[((enc_prev_r << 2) | s) & 0x0F];
        enc_prev_r = s;
    }
}

static void tim_pwm_init(TIM_HandleTypeDef *h, TIM_TypeDef *inst, uint32_t psc, uint32_t arr, uint8_t nch, uint32_t pulse)
{
    h->Instance = inst;
    h->Init.Prescaler = psc;
    h->Init.CounterMode = TIM_COUNTERMODE_UP;
    h->Init.Period = arr;
    h->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    h->Init.RepetitionCounter = 0;
    h->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(h);

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_PWM1; oc.Pulse = pulse;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH; oc.OCFastMode = TIM_OCFAST_DISABLE;
    for (uint8_t i = 1; i <= nch; i++) {
        uint32_t ch = (i == 1) ? TIM_CHANNEL_1 : (i == 2) ? TIM_CHANNEL_2 : (i == 3) ? TIM_CHANNEL_3 : TIM_CHANNEL_4;
        HAL_TIM_PWM_ConfigChannel(h, &oc, ch);
    }
}

void BSP_TimersInit(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    tim_pwm_init(&htim2, TIM2, 0, PWM_ARR, 4, 0);

    __HAL_RCC_TIM3_CLK_ENABLE();
    tim_pwm_init(&htim3, TIM3, 71, 19999, 3, 1500);

}

void BSP_Usart3Init(void)
{
    __HAL_RCC_USART3_CLK_ENABLE();
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
    HAL_NVIC_SetPriority(USART3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

void BSP_AdcInit(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    /* F103 ADC 时钟上限 14 MHz：PCLK2 72 MHz 必须 /6 = 12 MHz */
    __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);
    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    HAL_ADC_Init(&hadc1);
    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel = ADC_CHANNEL_5;
    ch.Rank = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;   /* 分压源阻抗 16.7k，用最长采样 */
    HAL_ADC_ConfigChannel(&hadc1, &ch);
}

static void BSP_Usart1Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
    /* Transmit_IT 靠 TXE 中断搬字节，必须开 NVIC + 提供中断入口，
       否则第一行就卡住、busy1 永久为 1 */
    HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/* 独立看门狗：LSI/64，重装 625 → 约 1 s（吸收队友方案）
 * F1 的 IWDG 只能开不能关，所以必须保证 1 ms 任务一直在跑 */
static void BSP_IwdgInit(void)
{
    DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;   /* 调试器暂停时冻结，便于单步 */
    IWDG->KR  = 0x5555U;                     /* 解锁 PR/RLR */
    IWDG->PR  = 4U;                          /* LSI/64 */
    IWDG->RLR = 625U;                        /* ≈1 s */
    IWDG->KR  = 0xAAAAU;                     /* 先喂一次 */
    IWDG->KR  = 0xCCCCU;                     /* 启动 */
}

void BSP_IwdgFeed(void) { IWDG->KR = 0xAAAAU; }

void BSP_Init(void)
{
    BSP_GpioInit();
    BSP_TimersInit();
    BSP_Usart3Init();
    BSP_Usart1Init();
    BSP_AdcInit();
    BSP_IwdgInit();
}