#include "buzzer.h"
#include "config.h"
#include "encoder.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "encoder_buzzer";

// Short original fanfare, played when the frequency reaches MIN_HZ
// "Sakura Sakura", traditional Japanese folk song
static const Note MELODY[] = {
    // Sa-ku-ra, sa-ku-ra
    {440, 400}, {440, 400}, {494, 800},
    {440, 400}, {440, 400}, {494, 800},
    // ya-yo-i no so-ra wa
    {440, 400}, {494, 400}, {523, 400}, {494, 400},
    {440, 400}, {494, 200}, {440, 200}, {349, 800},
    // mi-wa-ta-su ka-gi-ri
    {330, 400}, {262, 400}, {330, 400}, {349, 400},
    {330, 400}, {330, 200}, {262, 200}, {247, 800},
    // final: sa-ku-ra, sa-ku-ra, ...
    {440, 400}, {440, 400}, {494, 800},
    {440, 400}, {440, 400}, {494, 800},
    {330, 400}, {349, 400}, {494, 200}, {440, 200},
    {349, 400}, {330, 1200},
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
