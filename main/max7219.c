#include "max7219.h"
#include "esp_log.h"
#include <string.h>
#include <stdbool.h>

static const char *TAG = "MAX7219";

// 5x7 font data (each character is 5 columns wide)
// Characters are stored as 5 bytes, each byte is a column (LSB = top row)
static const uint8_t font_5x7[][5] = {
    // Space (32)
    {0x00, 0x00, 0x00, 0x00, 0x00},
    // ! (33)
    {0x00, 0x00, 0x5F, 0x00, 0x00},
    // " (34)
    {0x00, 0x07, 0x00, 0x07, 0x00},
    // # (35)
    {0x14, 0x7F, 0x14, 0x7F, 0x14},
    // $ (36)
    {0x24, 0x2A, 0x7F, 0x2A, 0x12},
    // % (37)
    {0x23, 0x13, 0x08, 0x64, 0x62},
    // & (38)
    {0x36, 0x49, 0x55, 0x22, 0x50},
    // ' (39)
    {0x00, 0x05, 0x03, 0x00, 0x00},
    // ( (40)
    {0x00, 0x1C, 0x22, 0x41, 0x00},
    // ) (41)
    {0x00, 0x41, 0x22, 0x1C, 0x00},
    // * (42)
    {0x08, 0x2A, 0x1C, 0x2A, 0x08},
    // + (43)
    {0x08, 0x08, 0x3E, 0x08, 0x08},
    // , (44)
    {0x00, 0x50, 0x30, 0x00, 0x00},
    // - (45)
    {0x08, 0x08, 0x08, 0x08, 0x08},
    // . (46)
    {0x00, 0x60, 0x60, 0x00, 0x00},
    // / (47)
    {0x20, 0x10, 0x08, 0x04, 0x02},
    // 0 (48)
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    // 1 (49)
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    // 2 (50)
    {0x42, 0x61, 0x51, 0x49, 0x46},
    // 3 (51)
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    // 4 (52)
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    // 5 (53)
    {0x27, 0x45, 0x45, 0x45, 0x39},
    // 6 (54)
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    // 7 (55)
    {0x01, 0x71, 0x09, 0x05, 0x03},
    // 8 (56)
    {0x36, 0x49, 0x49, 0x49, 0x36},
    // 9 (57)
    {0x06, 0x49, 0x49, 0x29, 0x1E},
    // : (58)
    {0x00, 0x36, 0x36, 0x00, 0x00},
    // ; (59)
    {0x00, 0x56, 0x36, 0x00, 0x00},
    // < (60)
    {0x00, 0x08, 0x14, 0x22, 0x41},
    // = (61)
    {0x14, 0x14, 0x14, 0x14, 0x14},
    // > (62)
    {0x41, 0x22, 0x14, 0x08, 0x00},
    // ? (63)
    {0x02, 0x01, 0x51, 0x09, 0x06},
    // @ (64)
    {0x32, 0x49, 0x79, 0x41, 0x3E},
    // A (65)
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    // B (66)
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    // C (67)
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    // D (68)
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    // E (69)
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    // F (70)
    {0x7F, 0x09, 0x09, 0x01, 0x01},
    // G (71)
    {0x3E, 0x41, 0x41, 0x51, 0x32},
    // H (72)
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    // I (73)
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    // J (74)
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    // K (75)
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    // L (76)
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    // M (77)
    {0x7F, 0x02, 0x04, 0x02, 0x7F},
    // N (78)
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    // O (79)
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    // P (80)
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    // Q (81)
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    // R (82)
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    // S (83)
    {0x46, 0x49, 0x49, 0x49, 0x31},
    // T (84)
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    // U (85)
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    // V (86)
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    // W (87)
    {0x7F, 0x20, 0x18, 0x20, 0x7F},
    // X (88)
    {0x63, 0x14, 0x08, 0x14, 0x63},
    // Y (89)
    {0x03, 0x04, 0x78, 0x04, 0x03},
    // Z (90)
    {0x61, 0x51, 0x49, 0x45, 0x43},
    // [ (91)
    {0x00, 0x00, 0x7F, 0x41, 0x41},
    // \ (92)
    {0x02, 0x04, 0x08, 0x10, 0x20},
    // ] (93)
    {0x41, 0x41, 0x7F, 0x00, 0x00},
    // ^ (94)
    {0x04, 0x02, 0x01, 0x02, 0x04},
    // _ (95)
    {0x40, 0x40, 0x40, 0x40, 0x40},
    // ` (96)
    {0x00, 0x01, 0x02, 0x04, 0x00},
    // a (97)
    {0x20, 0x54, 0x54, 0x54, 0x78},
    // b (98)
    {0x7F, 0x48, 0x44, 0x44, 0x38},
    // c (99)
    {0x38, 0x44, 0x44, 0x44, 0x20},
    // d (100)
    {0x38, 0x44, 0x44, 0x48, 0x7F},
    // e (101)
    {0x38, 0x54, 0x54, 0x54, 0x18},
    // f (102)
    {0x08, 0x7E, 0x09, 0x01, 0x02},
    // g (103)
    {0x08, 0x54, 0x54, 0x54, 0x3C},
    // h (104)
    {0x7F, 0x08, 0x04, 0x04, 0x78},
    // i (105)
    {0x00, 0x44, 0x7D, 0x40, 0x00},
    // j (106)
    {0x20, 0x40, 0x44, 0x3D, 0x00},
    // k (107)
    {0x00, 0x7F, 0x10, 0x28, 0x44},
    // l (108)
    {0x00, 0x41, 0x7F, 0x40, 0x00},
    // m (109)
    {0x7C, 0x04, 0x18, 0x04, 0x78},
    // n (110)
    {0x7C, 0x08, 0x04, 0x04, 0x78},
    // o (111)
    {0x38, 0x44, 0x44, 0x44, 0x38},
    // p (112)
    {0x7C, 0x14, 0x14, 0x14, 0x08},
    // q (113)
    {0x08, 0x14, 0x14, 0x18, 0x7C},
    // r (114)
    {0x7C, 0x08, 0x04, 0x04, 0x08},
    // s (115)
    {0x48, 0x54, 0x54, 0x54, 0x20},
    // t (116)
    {0x04, 0x3F, 0x44, 0x40, 0x20},
    // u (117)
    {0x3C, 0x40, 0x40, 0x20, 0x7C},
    // v (118)
    {0x1C, 0x20, 0x40, 0x20, 0x1C},
    // w (119)
    {0x3C, 0x40, 0x30, 0x40, 0x3C},
    // x (120)
    {0x44, 0x28, 0x10, 0x28, 0x44},
    // y (121)
    {0x0C, 0x50, 0x50, 0x50, 0x3C},
    // z (122)
    {0x44, 0x64, 0x54, 0x4C, 0x44},
    // { (123)
    {0x00, 0x08, 0x36, 0x41, 0x00},
    // | (124)
    {0x00, 0x00, 0x7F, 0x00, 0x00},
    // } (125)
    {0x00, 0x41, 0x36, 0x08, 0x00},
    // ~ (126)
    {0x08, 0x08, 0x2A, 0x1C, 0x08},
};

#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR 126
#define FONT_CHAR_WIDTH 5
#define FONT_CHAR_SPACING 1

// Send data to all chips in chain
static void max7219_send_to_all(max7219_t *dev, uint8_t reg, uint8_t data) {
    uint8_t tx_buf[MAX7219_NUM_CHIPS * 2];

    // Fill buffer with same command for all chips
    for (int i = 0; i < MAX7219_NUM_CHIPS; i++) {
        tx_buf[i * 2] = reg;
        tx_buf[i * 2 + 1] = data;
    }

    spi_transaction_t trans = {
        .length = MAX7219_NUM_CHIPS * 16,  // bits
        .tx_buffer = tx_buf,
    };

    spi_device_transmit(dev->spi_handle, &trans);
}

// Send a row of data (different data to each chip)
static void max7219_send_row(max7219_t *dev, uint8_t row, const uint8_t *data) {
    uint8_t tx_buf[MAX7219_NUM_CHIPS * 2];

    // Send to chips in reverse order (rightmost chip receives data first)
    for (int chip = 0; chip < MAX7219_NUM_CHIPS; chip++) {
       // int buf_idx = (MAX7219_NUM_CHIPS - 1 - chip) * 2;
        int buf_idx = chip*2;
        tx_buf[buf_idx] = MAX7219_REG_DIGIT0 + row;
        tx_buf[buf_idx + 1] = data[chip];
    }

    spi_transaction_t trans = {
        .length = MAX7219_NUM_CHIPS * 16,  // bits
        .tx_buffer = tx_buf,
    };

    spi_device_transmit(dev->spi_handle, &trans);
}

esp_err_t max7219_init(max7219_t *dev, const max7219_config_t *config) {
    esp_err_t ret;

    // Configure SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = config->pin_mosi,
        .miso_io_num = -1,  // Not used - MAX7219 is write-only
        .sclk_io_num = config->pin_clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = MAX7219_NUM_CHIPS * 2,
    };

    ret = spi_bus_initialize(config->spi_host, &bus_cfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure SPI device
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = config->clock_speed_hz,
        .mode = 0,  // CPOL=0, CPHA=0
        .spics_io_num = config->pin_cs,
        .queue_size = 1,
    };

    ret = spi_bus_add_device(config->spi_host, &dev_cfg, &dev->spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        spi_bus_free(config->spi_host);
        return ret;
    }

    // Store GPIO pins for ISR-safe bit-banging
    dev->pin_mosi = config->pin_mosi;
    dev->pin_clk = config->pin_clk;
    dev->pin_cs = config->pin_cs;

    // Initialize all MAX7219 chips
    max7219_send_to_all(dev, MAX7219_REG_DISPLAYTEST, 0x00);  // Normal operation
    max7219_send_to_all(dev, MAX7219_REG_SCANLIMIT, 0x07);    // Display all 8 digits
    max7219_send_to_all(dev, MAX7219_REG_DECODE, 0x00);       // No BCD decode
    max7219_send_to_all(dev, MAX7219_REG_INTENSITY, 0x08);    // Medium intensity
    max7219_send_to_all(dev, MAX7219_REG_SHUTDOWN, 0x01);     // Normal operation

    // Clear framebuffer and display
    memset(dev->framebuffer, 0, sizeof(dev->framebuffer));
    max7219_clear(dev);

    ESP_LOGI(TAG, "MAX7219 initialized with %d chips", MAX7219_NUM_CHIPS);
    return ESP_OK;
}

void max7219_set_intensity(max7219_t *dev, uint8_t intensity) {
    if (intensity > 15) intensity = 15;
    max7219_send_to_all(dev, MAX7219_REG_INTENSITY, intensity);
}

void max7219_clear(max7219_t *dev) {
    memset(dev->framebuffer, 0, sizeof(dev->framebuffer));
    for (int row = 0; row < 8; row++) {
        uint8_t zeros[MAX7219_NUM_CHIPS] = {0};
        max7219_send_row(dev, row, zeros);
    }
}

void max7219_refresh(max7219_t *dev) {
    // The MAX7219 expects data row by row, but our framebuffer is column-based
    for (int row = 0; row < 8; row++) {
        uint8_t row_data[MAX7219_NUM_CHIPS];
        for (int chip = 0; chip < MAX7219_NUM_CHIPS; chip++) {
            uint8_t byte = 0;
            // Build the row byte for this chip from 8 columns
            for (int col = 0; col < 8; col++) {
                int fb_col = chip * 8 + col;
                if (dev->framebuffer[fb_col] & (1 << row)) {
                    byte |= (1 << (7 - col));
                }
            }
            row_data[chip] = byte;
        }
        max7219_send_row(dev, row, row_data);
    }
}

void max7219_set_pixel(max7219_t *dev, uint8_t x, uint8_t y, uint8_t on) {
    if (x >= MAX7219_DISPLAY_WIDTH || y >= MAX7219_DISPLAY_HEIGHT) return;

    if (on) {
        dev->framebuffer[x] |= (1 << y);
    } else {
        dev->framebuffer[x] &= ~(1 << y);
    }
}

uint8_t max7219_draw_char(max7219_t *dev, int16_t x, char c) {
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) {
        c = ' ';
    }

    const uint8_t *glyph = font_5x7[c - FONT_FIRST_CHAR];

    for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
        int16_t px = x + col;
        if (px >= 0 && px < MAX7219_DISPLAY_WIDTH) {
            dev->framebuffer[px] = glyph[col];
        }
    }

    return FONT_CHAR_WIDTH;
}

void max7219_draw_string(max7219_t *dev, int16_t x, const char *str) {
    memset(dev->framebuffer, 0, sizeof(dev->framebuffer));

    while (*str) {
        uint8_t width = max7219_draw_char(dev, x, *str);
        x += width + FONT_CHAR_SPACING;
        str++;
    }
}

uint16_t max7219_get_string_width(const char *str) {
    uint16_t width = 0;
    while (*str) {
        width += FONT_CHAR_WIDTH + FONT_CHAR_SPACING;
        str++;
    }
    if (width > 0) {
        width -= FONT_CHAR_SPACING;  // Remove trailing space
    }
    return width;
}

void max7219_display_test(max7219_t *dev, bool enable) {
    max7219_send_to_all(dev, MAX7219_REG_DISPLAYTEST, enable ? 0x01 : 0x00);
}

void max7219_set_enabled(max7219_t *dev, bool enabled) {
    max7219_send_to_all(dev, MAX7219_REG_SHUTDOWN, enabled ? 0x01 : 0x00);
}

// ISR-safe version using GPIO bit-banging
// Sends shutdown register command to all chips without using SPI driver
void IRAM_ATTR max7219_set_enabled_isr(max7219_t *dev, bool enabled)
{
    uint8_t reg = MAX7219_REG_SHUTDOWN;
    uint8_t data = enabled ? 0x01 : 0x00;

    // Pull CS low to start transaction
    gpio_set_level(dev->pin_cs, 0);

    // Send 16 bits (reg + data) to each chip in chain
    // MSB first, clock data on rising edge
    for (int chip = 0; chip < MAX7219_NUM_CHIPS; chip++) {
        // Send register address (8 bits)
        for (int bit = 7; bit >= 0; bit--) {
            gpio_set_level(dev->pin_mosi, (reg >> bit) & 1);
            gpio_set_level(dev->pin_clk, 1);
            gpio_set_level(dev->pin_clk, 0);
        }
        // Send data (8 bits)
        for (int bit = 7; bit >= 0; bit--) {
            gpio_set_level(dev->pin_mosi, (data >> bit) & 1);
            gpio_set_level(dev->pin_clk, 1);
            gpio_set_level(dev->pin_clk, 0);
        }
    }

    // Pull CS high to latch data
    gpio_set_level(dev->pin_cs, 1);
}
