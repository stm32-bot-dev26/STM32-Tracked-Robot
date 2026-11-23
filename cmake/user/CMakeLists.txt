# 搴旂敤灞傦紙CubeMX 涓嶇敓鎴愩€佷笉瑕嗙洊鏈枃浠讹級
set(APP_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/../../App)
set(HAL_DRV  ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F1xx_HAL_Driver/Src)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${APP_DIR}/app.c
    ${APP_DIR}/bsp.c
    ${APP_DIR}/battery.c
    ${APP_DIR}/debug.c
    ${APP_DIR}/encoder.c
    ${APP_DIR}/gripper.c
    ${APP_DIR}/mixer.c
    ${APP_DIR}/motor.c
    ${APP_DIR}/rc.c
    ${APP_DIR}/servo.c
    
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${APP_DIR})

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${HAL_DRV}/stm32f1xx_hal_tim.c
    ${HAL_DRV}/stm32f1xx_hal_tim_ex.c
    ${HAL_DRV}/stm32f1xx_hal_uart.c
    ${HAL_DRV}/stm32f1xx_hal_adc.c
    ${HAL_DRV}/stm32f1xx_hal_adc_ex.c
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    HAL_TIM_MODULE_ENABLED
    HAL_UART_MODULE_ENABLED
    HAL_ADC_MODULE_ENABLED
)
