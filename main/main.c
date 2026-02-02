#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
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
// static const char *MESSAGE = "10:04";

// PWM brightness control (0-100, where 100 = full brightness at intensity 1)
// Lower values = dimmer. This allows going below the MAX7219's minimum intensity.
static uint8_t pwm_brightness = 50;  // Initial brightness before ADC takes over

// PWM cycle period in ms
#define PWM_PERIOD_MS 20


// Brightness tuning parameters (adjust these to taste)
// ADC readings: low voltage (bright room) = low reading, high voltage (dark room) = high reading
#define BRIGHTNESS_MIN      5    // Minimum brightness % (dark room)
#define BRIGHTNESS_MAX      100  // Maximum brightness % (bright room)
#define ADC_BRIGHT_LIMIT 100     // ADC reading in bright ambient light
#define ADC_DARK_LIMIT   3500    // ADC reading in dark ambient light

// Display task parameters
typedef struct {
    max7219_t *display;
    const char *message;
    adc_oneshot_unit_handle_t adc_handle;
} max2719_task_params_t;

//
//  Read ambient light level and map it to display brightness pwm value
//

static int jpwm =0;

int light2pwm(adc_oneshot_unit_handle_t adc_handle){
    // Read ambient light and update brightness
    int adc_reading = 0;
    if (adc_handle !=NULL && adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_reading) == ESP_OK) {
        // Clamp to thresholds
        if (adc_reading < ADC_BRIGHT_LIMIT) adc_reading = ADC_BRIGHT_LIMIT;
        if (adc_reading > ADC_DARK_LIMIT) adc_reading = ADC_DARK_LIMIT;

        // Map: photoresistor voltage divider output  to brightness:
        //     (low ADC) -> high brightness, dark (high ADC) -> low brightness
        int adc_range = ADC_DARK_LIMIT - ADC_BRIGHT_LIMIT;
        int brightness_range = BRIGHTNESS_MAX - BRIGHTNESS_MIN;
        uint8_t pwm = BRIGHTNESS_MAX - ((adc_reading - ADC_BRIGHT_LIMIT) * brightness_range / adc_range);
        if ((jpwm++)%1000 == 0)
            {   ESP_LOGI("ADC read fcn", "adc val: %d,  pwm: %d", adc_reading, pwm);
                fflush(stdout);
            }
        return pwm;
        }
    else return 0;
    }

static int kpwm = 0;
static void max7219_fixed_task(void *pvParameters)
{
    ESP_LOGI(TAG, "start of max7219_fixed_task()");
    fflush(stdout);

    max2719_task_params_t *params = (max2719_task_params_t *)pvParameters;
    max7219_t *display = params->display;
    const char *message = params->message;
    adc_oneshot_unit_handle_t adc_handle = params->adc_handle;

    uint16_t message_width = max7219_get_string_width(message);
    int16_t scroll_pos  = 1;   // Start at left most edge and draw txt from there.

    // Update display content (todo:  read from a message queue and move into loop)
    max7219_draw_string(display, scroll_pos, message);
    max7219_refresh(display);

    fflush(stdout);
    while(1){
            //  Figure out how bright display should be based on ambient light sensor
            pwm_brightness = light2pwm(adc_handle);

            // pw modulate the display
            int cycle_ms = PWM_PERIOD_MS;
            int on_time = (cycle_ms * pwm_brightness) / 100;
            int off_time = cycle_ms - on_time;
            if ((kpwm++)%50==0){ESP_LOGI(TAG, "ontime %d, offtime %d",on_time,off_time);
                fflush(stdout);
                }
            if (on_time > 0) {
                max7219_set_enabled(display, true);
                vTaskDelay(pdMS_TO_TICKS(on_time));
            }
            if (off_time > 0) {
                max7219_set_enabled(display, false);
                vTaskDelay(pdMS_TO_TICKS(off_time));
            }
            // ESP_LOGI(TAG, "finished pwm cycle");
            // fflush(stdout);
    }
}


//
// Combined display task - handles scrolling and PWM brightness
//

static void max7219_scrolling_task(void *pvParameters)
{   ESP_LOGI(TAG, "start of max7219_scrolling_task()");
    max2719_task_params_t *params = (max2719_task_params_t *)pvParameters;
    max7219_t *display = params->display;
    const char *message = params->message;
    adc_oneshot_unit_handle_t adc_handle = params->adc_handle;

    uint16_t message_width = max7219_get_string_width(message);
    int16_t scroll_pos = MAX7219_DISPLAY_WIDTH;

    int j=0;
    while (1) {
        j++;
        //  Figure out how bright display should be based on ambient light sensor
        pwm_brightness = light2pwm(adc_handle);

        // Update display content
        max7219_draw_string(display, scroll_pos, message);
        max7219_refresh(display);

        // Move scroll position
        scroll_pos--;
        if (scroll_pos < -(int16_t)message_width) {
            scroll_pos = MAX7219_DISPLAY_WIDTH;
        }

        // PWM delay loop for scroll timing + brightness control
        int remaining_ms = SCROLL_DELAY_MS;
        while (remaining_ms > 0) {
            int cycle_ms = (remaining_ms < PWM_PERIOD_MS) ? remaining_ms : PWM_PERIOD_MS;
            int on_time = (cycle_ms * pwm_brightness) / 100;
            int off_time = cycle_ms - on_time;

            if (on_time > 0) {
                max7219_set_enabled(display, true);
                vTaskDelay(pdMS_TO_TICKS(on_time));
            }
            if (off_time > 0) {
                max7219_set_enabled(display, false);
                vTaskDelay(pdMS_TO_TICKS(off_time));
            }
            remaining_ms -= cycle_ms;
        }
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
        .clock_speed_hz = 10 * 1000,  // 10 kHz for debugging
    };

    esp_err_t ret = max7219_init(&display, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }

    max7219_set_intensity(&display, 1);  // Lowest hardware intensity

    ESP_LOGI(TAG, "Display initialized");

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

    // Start combined display task (handles scrolling + PWM brightness + light sensing)
    static max2719_task_params_t params;
    params.display = &display;
    params.message = MESSAGE;
    params.adc_handle = adc_handle;
    // xTaskCreate(max7219_scrolling_task, "max7219_scrolling", 2048, &params, 5, NULL);
    params.message = "09:17";
    xTaskCreate(max7219_fixed_task, "max7219_fixed", 2048, &params, 5, NULL);
    ESP_LOGI(TAG, "display task started ... ");
    vTaskDelay(30);
}
