#ifndef MAX7219_H
#define MAX7219_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

// MAX7219 Register addresses
#define MAX7219_REG_NOOP        0x00
#define MAX7219_REG_DIGIT0      0x01
#define MAX7219_REG_DIGIT1      0x02
#define MAX7219_REG_DIGIT2      0x03
#define MAX7219_REG_DIGIT3      0x04
#define MAX7219_REG_DIGIT4      0x05
#define MAX7219_REG_DIGIT5      0x06
#define MAX7219_REG_DIGIT6      0x07
#define MAX7219_REG_DIGIT7      0x08
#define MAX7219_REG_DECODE      0x09
#define MAX7219_REG_INTENSITY   0x0A
#define MAX7219_REG_SCANLIMIT   0x0B
#define MAX7219_REG_SHUTDOWN    0x0C
#define MAX7219_REG_DISPLAYTEST 0x0F

// Configuration for 4-chip chain (32x8 display)
#define MAX7219_NUM_CHIPS       4
#define MAX7219_DISPLAY_WIDTH   32  // 4 chips * 8 columns
#define MAX7219_DISPLAY_HEIGHT  8

typedef struct {
    gpio_num_t pin_mosi;    // DIN
    gpio_num_t pin_clk;     // CLK
    gpio_num_t pin_cs;      // CS
    spi_host_device_t spi_host;
    int clock_speed_hz;     // SPI clock speed (max 10MHz for MAX7219)
} max7219_config_t;

typedef struct {
    spi_device_handle_t spi_handle;
    uint8_t framebuffer[MAX7219_DISPLAY_WIDTH];  // Column-based framebuffer
} max7219_t;

// Initialize the MAX7219 chain
esp_err_t max7219_init(max7219_t *dev, const max7219_config_t *config);

// Set display intensity (0-15)
void max7219_set_intensity(max7219_t *dev, uint8_t intensity);

// Clear the display
void max7219_clear(max7219_t *dev);

// Update display from framebuffer
void max7219_refresh(max7219_t *dev);

// Set a single pixel
void max7219_set_pixel(max7219_t *dev, uint8_t x, uint8_t y, uint8_t on);

// Draw a character at position, returns width of character
uint8_t max7219_draw_char(max7219_t *dev, int16_t x, char c);

// Draw a string at position
void max7219_draw_string(max7219_t *dev, int16_t x, const char *str);

// Get the pixel width of a string
uint16_t max7219_get_string_width(const char *str);

// Display test mode (lights all LEDs when enabled)
void max7219_display_test(max7219_t *dev, bool enable);

#endif // MAX7219_H
