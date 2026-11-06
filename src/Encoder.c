#include "encoder.h"
#include "config.h"

#include "driver/gptimer.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

// Written by the timer interrupt
static volatile int32_t count = 0;
static uint8_t lastState = 0;
static portMUX_TYPE countLock = portMUX_INITIALIZER_UNLOCKED;

// Count change for each (old state, new state). 0 = no move or bounce.
static const int8_t MOVES[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static uint8_t readState(void)
{
    return (gpio_get_level(ENCODER_CLK_PIN) << 1) | gpio_get_level(ENCODER_DT_PIN);
}

// Hardware timer, every 1 ms
static bool onTimer(gptimer_handle_t timer, const gptimer_alarm_event_data_t *event, void *arg)
{
    const uint8_t state = readState();

    portENTER_CRITICAL_ISR(&countLock);
    count += MOVES[(lastState << 2) | state];
    portEXIT_CRITICAL_ISR(&countLock);

    lastState = state;
    return false;
}

void encoderStart(void)
{
    gpio_config_t pins = {
        .pin_bit_mask = (1ULL << ENCODER_CLK_PIN) | (1ULL << ENCODER_DT_PIN) | (1ULL << ENCODER_SW_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&pins));
    lastState = readState();

    gptimer_handle_t timer = NULL;
    gptimer_config_t timerConfig = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,  // 1 tick = 1 us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timerConfig, &timer));

    gptimer_event_callbacks_t callbacks = { .on_alarm = onTimer };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer, &callbacks, NULL));

    gptimer_alarm_config_t alarm = {
        .alarm_count = ENCODER_READ_US,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer, &alarm));

    ESP_ERROR_CHECK(gptimer_enable(timer));
    ESP_ERROR_CHECK(gptimer_start(timer));
}

int32_t encoderSteps(void)
{
    portENTER_CRITICAL(&countLock);
    const int32_t value = count;
    portEXIT_CRITICAL(&countLock);

    return value / COUNTS_PER_STEP;
}

bool encoderButtonPressed(void)
{
    static int lastRead = 1;
    static int stableState = 1;
    static int64_t lastChangeUs = 0;

    const int read = gpio_get_level(ENCODER_SW_PIN);
    const int64_t now = esp_timer_get_time();

    if (read != lastRead) {
        lastRead = read;
        lastChangeUs = now;
        return false;
    }

    if (read != stableState && now - lastChangeUs >= DEBOUNCE_MS * 1000) {
        stableState = read;
        return read == 0;  // pressed = LOW
    }
    return false;
}
