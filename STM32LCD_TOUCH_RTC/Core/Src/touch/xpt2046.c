#include "xpt2046.h"
#include "stm32f4xx_hal.h"
#include "lvgl.h"
//#include "touch_calib.h"

/* Ajusta se usares outro SPI */
extern SPI_HandleTypeDef hspi2;

/* AJUSTA OS PINOS SE NECESSÁRIO */
#define XPT_CS_LOW()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET)
#define XPT_CS_HIGH()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET)

uint8_t XPT2046_Pressed(void)
{
    /* PEN é ativo a LOW */
    return (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5) == GPIO_PIN_RESET);
}

static uint16_t xpt_read(uint8_t cmd)
{
    uint8_t tx = cmd;
    uint8_t rx[2] = {0};

    XPT_CS_LOW();
    HAL_SPI_Transmit(&hspi2, &tx, 1, 100);
    HAL_SPI_Receive(&hspi2, rx, 2, 100);
    XPT_CS_HIGH();

    return ((rx[0] << 8) | rx[1]) >> 4; // 12 bits
}

void XPT2046_Read(uint16_t *x, uint16_t *y)
{
    /* Ordem mais comum */
    *y = xpt_read(0xD0); // Y
    *x = xpt_read(0x90); // X
}

