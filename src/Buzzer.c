#include "buzzer.h"
#include "config.h"

#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MODE        LEDC_LOW_SPEED_MODE  // ESP32-S3 has only low speed mode
#define TIMER       LEDC_TIMER_0
#define CHANNEL     LEDC_CHANNEL_0
#define HALF_DUTY   512                  // 50% of 10-bit duty: square wave

void buzzerStart(uint32_t hz)
{
    ledc_timer_config_t timer = {
        .speed_mode = MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = TIMER,
        .freq_hz = hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .gpio_num = BUZZER_PIN,
        .speed_mode = MODE,
        .channel = CHANNEL,
        .timer_sel = TIMER,
        .duty = HALF_DUTY,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

void buzzerSetFrequency(uint32_t hz)
{
    ESP_ERROR_CHECK(ledc_set_freq(MODE, TIMER, hz));
}

void buzzerSetSound(bool on)
{
    ESP_ERROR_CHECK(ledc_set_duty(MODE, CHANNEL, on ? HALF_DUTY : 0));
    ESP_ERROR_CHECK(ledc_update_duty(MODE, CHANNEL));
}

void buzzerPlay(const Note *notes, size_t count, uint32_t backToHz)
{
    for (size_t i = 0; i < count; i++) {
        if (notes[i].hz == 0) {
            buzzerSetSound(false);
        } else {
            buzzerSetFrequency(notes[i].hz);
            buzzerSetSound(true);
        }
        vTaskDelay(pdMS_TO_TICKS(notes[i].ms));
    }

    buzzerSetFrequency(backToHz);
    buzzerSetSound(true);
}
