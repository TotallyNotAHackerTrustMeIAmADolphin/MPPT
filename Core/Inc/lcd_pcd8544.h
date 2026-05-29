#ifndef __LCD_PCD8544_H
#define __LCD_PCD8544_H

#include "stm32f0xx_hal.h"
#include <stdbool.h>

/* Nokia 5110 (PCD8544) Dimensions */
#define LCD_WIDTH  84
#define LCD_HEIGHT 48
#define LCD_BUFFER_SIZE (LCD_WIDTH * LCD_HEIGHT / 8)

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *port_ce;
    uint16_t pin_ce;
    GPIO_TypeDef *port_dc;
    uint16_t pin_dc;
    GPIO_TypeDef *port_rst;
    uint16_t pin_rst;
    uint8_t buffer[LCD_BUFFER_SIZE];
} LCD_Handle_t;

void LCD_Init(LCD_Handle_t *hlcd);
void LCD_Clear(LCD_Handle_t *hlcd);
void LCD_Update(LCD_Handle_t *hlcd);
void LCD_DrawPixel(LCD_Handle_t *hlcd, uint8_t x, uint8_t y, bool on);
void LCD_DrawString(LCD_Handle_t *hlcd, uint8_t x, uint8_t y, const char *str);

#endif
