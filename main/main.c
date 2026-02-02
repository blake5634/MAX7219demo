#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gptimer.h"
#include "max7219.h"

static const char *TAG = "MAX7219_DEMO";

// forward declarations
static void max7219_scrolling_task(void*);
static void max7219_fixed_task(void*);
int light2pwm(adc_oneshot_unit_handle_t);

// Pin definitions for ESP32-C6
#define PIN_MOSI    GPIO_NUM_0   // DIN
#define PIN_CS      GPIO_NUM_2   // CS
#define PIN_CLK     GPIO_NUM_4   // CLK
#define PIN_PHOTORESISTOR  GPIO_NUM_5  // ADC input for ambient light sensor
#define ADC_CHANNEL ADC_CHANNEL_5      // GPIO5 = ADC1 channel 5 on ESP32-C6

// Scroll speed (lower = faster)
#define SCROLL_DELAY_MS 200

// Message to display
static const char *MESSAGE = "Hello from Claude!   ";

// ============================================================================
// Hardware Timer PWM Configuration
// ============================================================================
// PWM frequency in Hz (200 Hz = 5ms period, well above flicker threshold)
#define PWM_FREQUENCY_HZ    200
#define PWM_PERIOD_US       (1000000 / PWM_FREQUENCY_HZ)  // 5000us = 5ms

// Minimum on/off time in microseconds (to avoid very short pulses)
#define PWM_MIN_PULSE_US    50

// PWM timing (in microseconds), pre-computed by tasks, used by ISR
static volatile uint32_t pwm_on_time_us = PWM_PERIOD_US / 2;   // Initial 50%
static volatile uint32_t pwm_off_time_us = PWM_PERIOD_US / 2;

// Timer ISR state machine
typedef enum {
    PWM_STATE_ON,
    PWM_STATE_OFF
} pwm_state_t;

static volatile pwm_state_t pwm_state = PWM_STATE_ON;
static max7219_t *pwm_display = NULL;
static gptimer_handle_t pwm_timer = NULL;

// Helper function for tasks to update PWM timing from brightness percentage
static inline void pwm_set_brightness(uint8_t brightness_percent)
{
    if (brightness_percent > 100) brightness_percent = 100;

    uint32_t on_time = (PWM_PERIOD_US * brightness_percent) / 100;
    uint32_t off_time = PWM_PERIOD_US - on_time;

    // Enforce minimum pulse widths
    if (on_time < PWM_MIN_PULSE_US && brightness_percent > 0) on_time = PWM_MIN_PULSE_US;
    if (off_time < PWM_MIN_PULSE_US && brightness_percent < 100) off_time = PWM_MIN_PULSE_US;

    // Atomic-ish update (single word writes are atomic on ESP32)
    pwm_on_time_us = on_time;
    pwm_off_time_us = off_time;
}

// Brightness tuning parameters
#define BRIGHTNESS_MIN      5    // Minimum brightness % (dark room)
#define BRIGHTNESS_MAX      100  // Maximum brightness % (bright room)
#define ADC_BRIGHT_LIMIT    100  // ADC reading in bright ambient light
#define ADC_DARK_LIMIT      3500 // ADC reading in dark ambient light

// Display task parameters
typedef struct {
    max7219_t *display;
    const char *message;
    adc_oneshot_unit_handle_t adc_handle;
} max2719_task_params_t;

//
//  Read ambient light level and map it to display brightness pwm value
//
int light2pwm(adc_oneshot_unit_handle_t adc_handle)
{
    int adc_reading = 0;
    if (adc_handle != NULL && adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_reading) == ESP_OK) {
        // Clamp to thresholds
        if (adc_reading < ADC_BRIGHT_LIMIT) adc_reading = ADC_BRIGHT_LIMIT;
        if (adc_reading > ADC_DARK_LIMIT) adc_reading = ADC_DARK_LIMIT;

        // Map: bright (low ADC) -> high brightness, dark (high ADC) -> low brightness
        int adc_range = ADC_DARK_LIMIT - ADC_BRIGHT_LIMIT;
        int brightness_range = BRIGHTNESS_MAX - BRIGHTNESS_MIN;
        uint8_t brightness = BRIGHTNESS_MAX - ((adc_reading - ADC_BRIGHT_LIMIT) * brightness_range / adc_range);
        return brightness;
    }
    return BRIGHTNESS_MIN;
}

// ============================================================================
// Hardware Timer PWM ISR - runs only twice per PWM cycle (on edge and off edge)
// ============================================================================
static bool IRAM_ATTR pwm_timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    uint64_t next_alarm_us;

    if (pwm_state == PWM_STATE_ON) {
        // Turn display OFF, schedule next ON
        max7219_set_enabled_isr(pwm_display, false);
        pwm_state = PWM_STATE_OFF;
        next_alarm_us = edata->alarm_value + pwm_off_time_us;
    } else {
        // Turn display ON, schedule next OFF
        max7219_set_enabled_isr(pwm_display, true);
        pwm_state = PWM_STATE_ON;
        next_alarm_us = edata->alarm_value + pwm_on_time_us;
    }

    // Set next alarm
    gptimer_alarm_config_t alarm_cfg = {
        .alarm_count = next_alarm_us,
        .flags.auto_reload_on_alarm = false,
    };
    gptimer_set_alarm_action(timer, &alarm_cfg);

    return false;  // No need to yield to higher priority task
}

// Initialize hardware PWM timer
static esp_err_t pwm_timer_init(max7219_t *display)
{
    pwm_display = display;

    // Create timer with 1MHz resolution (1us ticks)
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,  // 1MHz = 1us resolution
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &pwm_timer));

    // Register ISR callback
    gptimer_event_callbacks_t cbs = {
        .on_alarm = pwm_timer_isr,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(pwm_timer, &cbs, NULL));

    // Enable and start timer
    ESP_ERROR_CHECK(gptimer_enable(pwm_timer));
    ESP_ERROR_CHECK(gptimer_start(pwm_timer));

    // Set initial alarm using pre-computed on time
    gptimer_alarm_config_t alarm_cfg = {
        .alarm_count = pwm_on_time_us,
        .flags.auto_reload_on_alarm = false,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(pwm_timer, &alarm_cfg));

    // Ensure display starts ON
    max7219_set_enabled(display, true);
    pwm_state = PWM_STATE_ON;

    ESP_LOGI(TAG, "Hardware PWM initialized: %d Hz, %dus period", PWM_FREQUENCY_HZ, PWM_PERIOD_US);
    return ESP_OK;
}

// ============================================================================
// Display Tasks - simplified, PWM handled by hardware timer
// ============================================================================

static void max7219_fixed_task(void *pvParameters)
{
    ESP_LOGI(TAG, "start of max7219_fixed_task()");

    max2719_task_params_t *params = (max2719_task_params_t *)pvParameters;
    max7219_t *display = params->display;
    const char *message = params->message;
    adc_oneshot_unit_handle_t adc_handle = params->adc_handle;

    // Draw message once (static display)
    max7219_draw_string(display, 1, message);
    max7219_refresh(display);

    while (1) {
        // Update brightness from ambient light sensor
        uint8_t brightness = light2pwm(adc_handle);
        pwm_set_brightness(brightness);
        ESP_LOGI(TAG, "brightness set to: %d",brightness);
        // Sleep before next brightness update (100ms = 10 updates/sec is plenty)
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}


// Scrolling display task - PWM handled by hardware timer
static void max7219_scrolling_task(void *pvParameters)
{
    ESP_LOGI(TAG, "start of max7219_scrolling_task()");

    max2719_task_params_t *params = (max2719_task_params_t *)pvParameters;
    max7219_t *display = params->display;
    const char *message = params->message;
    adc_oneshot_unit_handle_t adc_handle = params->adc_handle;

    uint16_t message_width = max7219_get_string_width(message);
    int16_t scroll_pos = MAX7219_DISPLAY_WIDTH;

    while (1) {
        // Update brightness from ambient light sensor
        uint8_t brightness = light2pwm(adc_handle);
        pwm_set_brightness(brightness);

        // Update display content
        max7219_draw_string(display, scroll_pos, message);
        max7219_refresh(display);

        // Move scroll position
        scroll_pos--;
        if (scroll_pos < -(int16_t)message_width) {
            scroll_pos = MAX7219_DISPLAY_WIDTH;
        }

        // Simple delay for scroll timing - PWM runs independently in hardware
        vTaskDelay(pdMS_TO_TICKS(SCROLL_DELAY_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "MAX7219 32x8 LED Matrix Demo (Hardware SPI)");
    ESP_LOGI(TAG, "Pins: MOSI=%d, CS=%d, CLK=%d", PIN_MOSI, PIN_CS, PIN_CLK);

    // Initialize the MAX7219 display
    static max7219_t display;
    max7219_config_t config = {
        .pin_mosi = PIN_MOSI,
        .pin_clk = PIN_CLK,
        .pin_cs = PIN_CS,
        .spi_host = SPI2_HOST,
        .clock_speed_hz = 2 * 1000 * 1000,  // 2 MHz for fast ISR updates
    };

    esp_err_t ret = max7219_init(&display, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }

    max7219_set_intensity(&display, 1);  // Lowest hardware intensity
    ESP_LOGI(TAG, "Display initialized");

    // Initialize hardware PWM timer
    ret = pwm_timer_init(&display);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize PWM timer");
        return;
    }

    // Initialize ADC for photoresistor
    adc_oneshot_unit_handle_t adc_handle = NULL;
    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = ADC_UNIT_1,
    };
    ret = adc_oneshot_new_unit(&adc_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC: %s", esp_err_to_name(ret));
        return;
    }

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,  // Full range ~0-3.3V
    };
    ret = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "ADC initialized on GPIO %d", PIN_PHOTORESISTOR);

    ESP_LOGI(TAG, "Scrolling message: \"%s\"", MESSAGE);

    // Start display task (PWM brightness handled by hardware timer)
    static max2719_task_params_t params;
    params.display = &display;
    params.message = MESSAGE;
    params.adc_handle = adc_handle;
    // xTaskCreate(max7219_scrolling_task, "max7219_scrolling", 2048, &params, 5, NULL);
    params.message = "09:17";
    xTaskCreate(max7219_fixed_task, "max7219_fixed", 2048, &params, 5, NULL);
    ESP_LOGI(TAG, "Display task started");
}
