#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "max7219.h"

static const char *TAG = "MAX7219_DEMO";

// Pin definitions for ESP32-C6
#define PIN_MOSI    GPIO_NUM_0   // DIN
#define PIN_CS      GPIO_NUM_2   // CS
#define PIN_CLK     GPIO_NUM_4   // CLK

// Scroll speed (lower = faster)
#define SCROLL_DELAY_MS 50

// Message to display
static const char *MESSAGE = "Hello from Claude!   ";


/*
// TESTING

  void app_main(void)
  {
      gpio_set_direction(GPIO_NUM_0, GPIO_MODE_OUTPUT);
      while(1) {
          gpio_set_level(GPIO_NUM_0, 1);
          vTaskDelay(pdMS_TO_TICKS(500));
          gpio_set_level(GPIO_NUM_0, 0);
          vTaskDelay(pdMS_TO_TICKS(500));
      }
  }
//END TESTING
*/

void app_main(void)
{
    ESP_LOGI(TAG, "MAX7219 32x8 LED Matrix Demo (Hardware SPI)");
    ESP_LOGI(TAG, "Pins: MOSI=%d, CS=%d, CLK=%d", PIN_MOSI, PIN_CS, PIN_CLK);

    // Initialize the MAX7219 display
    max7219_t display;
    max7219_config_t config = {
        .pin_mosi = PIN_MOSI,
        .pin_clk = PIN_CLK,
        .pin_cs = PIN_CS,
        .spi_host = SPI2_HOST,
        .clock_speed_hz = 10 * 1000,  // 10 MHz (MAX7219 max)
    };

    esp_err_t ret = max7219_init(&display, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }

    max7219_set_intensity(&display, 5);  // Lower intensity to save power

    ESP_LOGI(TAG, "Display initialized");

    // Hardware test: light all LEDs for 2 seconds
    ESP_LOGI(TAG, "Running display test (all LEDs on)...");
    max7219_display_test(&display, true);
    vTaskDelay(pdMS_TO_TICKS(2000));
    max7219_display_test(&display, false);
    ESP_LOGI(TAG, "Display test complete");

    ESP_LOGI(TAG, "Scrolling message: \"%s\"", MESSAGE);

    // Calculate message width for scrolling
    uint16_t message_width = max7219_get_string_width(MESSAGE);
    int16_t scroll_pos = MAX7219_DISPLAY_WIDTH;  // Start from right edge

    // Scrolling loop
    while (1) {
        // Draw the message at current scroll position
        max7219_draw_string(&display, scroll_pos, MESSAGE);
        max7219_refresh(&display);

        // Move scroll position
        scroll_pos--;

        // Reset when message has scrolled completely off the left
        if (scroll_pos < -(int16_t)message_width) {
            scroll_pos = MAX7219_DISPLAY_WIDTH;
        }

        vTaskDelay(pdMS_TO_TICKS(SCROLL_DELAY_MS));
    }
}
