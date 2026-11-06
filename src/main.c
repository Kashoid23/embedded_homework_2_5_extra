#include "buzzer.h"
#include "config.h"
#include "encoder.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "encoder_buzzer";

// Short original fanfare, played when the frequency reaches MIN_HZ
static const Note MELODY[] = {
    {523, 120}, {659, 120}, {784, 120}, {1047, 240},
    {0, 80},
    {784, 120}, {1047, 400},
};
#define MELODY_LENGTH (sizeof(MELODY) / sizeof(MELODY[0]))

void app_main(void)
{
    encoderStart();
    buzzerStart(START_HZ);

    int32_t lastSteps = 0;
    int32_t hz = START_HZ;
    bool soundOn = true;

    ESP_LOGI(TAG, "Start: %d Hz", START_HZ);

    while (1) {
        const int32_t steps = encoderSteps();

        if (steps != lastSteps) {
            const int32_t oldHz = hz;

            hz += (steps - lastSteps) * STEP_HZ;
            if (hz < MIN_HZ) hz = MIN_HZ;
            if (hz > MAX_HZ) hz = MAX_HZ;

            buzzerSetFrequency(hz);
            ESP_LOGI(TAG, "Frequency: %ld Hz", (long)hz);

            // Just reached the minimum: play the melody
            if (hz == MIN_HZ && oldHz != MIN_HZ && soundOn) {
                ESP_LOGI(TAG, "Minimum reached: melody");
                buzzerPlay(MELODY, MELODY_LENGTH, hz);
            }

            lastSteps = encoderSteps();  // ignore turns made during the melody
        }

        if (encoderButtonPressed()) {
            soundOn = !soundOn;
            buzzerSetSound(soundOn);
            ESP_LOGI(TAG, "Sound: %s", soundOn ? "on" : "off");
        }

        vTaskDelay(pdMS_TO_TICKS(5));  // let other FreeRTOS tasks run
    }
}
