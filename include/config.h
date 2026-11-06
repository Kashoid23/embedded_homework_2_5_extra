#pragma once

#include "driver/gpio.h"

// Pins
#define ENCODER_CLK_PIN  GPIO_NUM_15
#define ENCODER_DT_PIN   GPIO_NUM_16
#define ENCODER_SW_PIN   GPIO_NUM_17
#define BUZZER_PIN       GPIO_NUM_18

// Encoder
#define ENCODER_READ_US  1000  // hardware timer reads the encoder every 1 ms
#define COUNTS_PER_STEP  4     // KY-040: 4 signal changes per click

// Buzzer frequency
#define START_HZ  1000
#define STEP_HZ   100   // change per encoder click
#define MIN_HZ    200   // reaching it plays the melody
#define MAX_HZ    4000

// Button
#define DEBOUNCE_MS  30
