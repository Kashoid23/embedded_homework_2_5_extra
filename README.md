## Обладнання

- **ESP32-S3 N16R8**
- енкодер **KY-040** (CLK, DT, SW)
- пасивний базер

## Схема підключення

<img width="2720" height="1600" alt="esp32s3_encoder_buzzer_schematic" src="https://github.com/user-attachments/assets/588d017e-7ee7-4eca-93a4-29412ac0a7ca" />

## Як це працює

### 1. Старт: `app_main()`

У ESP-IDF програма починається з `app_main()`, а не з `main()` чи `setup()`. Її викликає FreeRTOS як звичайну задачу.

```c
encoderStart();
buzzerStart(START_HZ);
```

Спершу запускаються енкодер і базер. Потім готуються три змінні:

```c
int32_t lastSteps = 0;      // скільки кліків було минулого разу
int32_t hz = START_HZ;      // поточна частота
bool soundOn = true;        // чи увімкнений звук
```

### 2. Енкодер: `encoderStart()` (`encoder.c`)

#### 2.1. Піни

```c
gpio_config_t pins = {
    .pin_bit_mask = (1ULL << ENCODER_CLK_PIN) | (1ULL << ENCODER_DT_PIN) | (1ULL << ENCODER_SW_PIN),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
};
```

CLK, DT і SW налаштовуються як входи з підтяжкою вгору. `pin_bit_mask` — це бітова маска: кожен біт відповідає одному піну, тож одним викликом налаштовуються одразу три.

```c
lastState = readState();
```

Запам'ятовуємо початковий стан енкодера, щоб перше порівняння було правильним.

#### 2.2. Апаратний таймер

```c
gptimer_config_t timerConfig = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 1000000,
};
gptimer_new_timer(&timerConfig, &timer);
```

Створюється таймер, який рахує вгору з частотою 1 МГц, тобто 1 такт = 1 мкс.

```c
gptimer_event_callbacks_t callbacks = { .on_alarm = onTimer };
gptimer_register_event_callbacks(timer, &callbacks, NULL);
```

Тут ми кажемо таймеру викликати `onTimer()`, коли спрацює будильник.

```c
gptimer_alarm_config_t alarm = {
    .alarm_count = ENCODER_READ_US,           // 1000 тактів = 1 мс
    .flags.auto_reload_on_alarm = true,       // після будильника почати з 0
};
```

Будильник спрацьовує кожні 1000 мкс. Після цього лічильник сам обнуляється, тож `onTimer()` викликається кожну 1 мс вічно.

```c
gptimer_enable(timer);
gptimer_start(timer);
```

Вмикаємо і запускаємо таймер. Тепер енкодер читається апаратно, незалежно від головного циклу.

### 3. Як `onTimer()` визначає поворот

Енкодер має два виходи, CLK і DT. Коли його крутять, вони перемикаються зі зсувом, а порядок перемикання показує напрямок:

```
за годинниковою:   CLK DT: 11 → 01 → 00 → 10 → 11
проти:             CLK DT: 11 → 10 → 00 → 01 → 11
```

```c
static uint8_t readState(void)
{
    return (gpio_get_level(ENCODER_CLK_PIN) << 1) | gpio_get_level(ENCODER_DT_PIN);
}
```

Два біти складаються в одне число від 0 до 3: CLK старший біт, DT молодший.

```c
const uint8_t state = readState();
count += MOVES[(lastState << 2) | state];
lastState = state;
```

1. Зчитуємо новий стан.
2. Склеюємо старий і новий стан у число від 0 до 15. Це номер переходу.
3. Таблиця `MOVES` за цим номером каже, що сталося:
   - `+1` — крок в один бік;
   - `−1` — крок в інший;
   - `0` — нічого не змінилось або перехід неможливий, тобто брязкіт.
4. Запам'ятовуємо новий стан для наступного разу.

Наприклад, перехід `11 → 01`: старий стан 3, новий 1, номер `3 << 2 | 1 = 13`, `MOVES[13] = 1`, отже крок `+1`.

```c
portENTER_CRITICAL_ISR(&countLock);
...
portEXIT_CRITICAL_ISR(&countLock);
```

Критична секція захищає `count`, щоб головна задача не прочитала значення посеред запису.

### 4. Кліки: `encoderSteps()`

```c
return value / COUNTS_PER_STEP;
```

KY-040 за один клік проходить повний цикл із 4 переходів, тобто `count` змінюється на 4. Ділимо на 4 і отримуємо кількість кліків.

### 5. Базер: `buzzerStart()` (`buzzer.c`)

LEDC — апаратний генератор ШІМ. Він має дві частини.

**Таймер LEDC задає частоту:**
```c
ledc_timer_config_t timer = {
    .duty_resolution = LEDC_TIMER_10_BIT,   // скважність 0..1023
    .freq_hz = hz,                          // 1000 Гц на старті
    ...
};
```

**Канал LEDC виводить сигнал на пін:**
```c
ledc_channel_config_t channel = {
    .gpio_num = BUZZER_PIN,
    .duty = HALF_DUTY,                      // 512 з 1023 = 50%
    ...
};
```

Скважність 50 % дає меандр: половину періоду `HIGH`, половину `LOW`. Пасивний базер звучить на частоті цього меандру.

Три функції для керування:

| Функція | Що робить |
|---|---|
| `buzzerSetFrequency(hz)` | змінює частоту таймера LEDC, тобто висоту звуку |
| `buzzerSetSound(on)` | скважність 50 % — звук є, 0 % — тиша |
| `buzzerPlay(notes, count, backToHz)` | грає мелодію нота за нотою |

### 6. Головний цикл

#### Крок 1. Чи крутили енкодер?

```c
const int32_t steps = encoderSteps();
if (steps != lastSteps) {
```

Порівнюємо кількість кліків зараз і минулого разу.

#### Крок 2. Нова частота

```c
const int32_t oldHz = hz;

hz += (steps - lastSteps) * STEP_HZ;
if (hz < MIN_HZ) hz = MIN_HZ;
if (hz > MAX_HZ) hz = MAX_HZ;
```

- `steps - lastSteps` — скільки кліків зроблено з минулої перевірки: +1, −2 тощо.
- Кожен клік змінює частоту на 100 Гц.
- Результат обмежується межами 200–4000 Гц.

Частота рахується від різниці кліків, а не від їх загальної кількості. Тому якщо прокрутити далеко за мінімум, достатньо одного кліку назад, щоб частота знову почала рости.

```c
buzzerSetFrequency(hz);
ESP_LOGI(TAG, "Frequency: %ld Hz", (long)hz);
```

Застосовуємо частоту і пишемо її в монітор порту.

#### Крок 3. Мелодія на мінімумі

```c
if (hz == MIN_HZ && oldHz != MIN_HZ && soundOn) {
    buzzerPlay(MELODY, MELODY_LENGTH, hz);
}
```

Мелодія грає, лише якщо виконано всі три умови:
- частота **зараз** мінімальна;
- **до цього** вона не була мінімальною, тобто ми щойно дійшли до мінімуму, а не стоїмо на ньому;
- звук увімкнений.

```c
lastSteps = encoderSteps();
```

Під час мелодії енкодер міг повертатися, і таймер ці кліки порахував. Беремо поточне значення, щоб після мелодії частота не стрибнула.

#### Крок 4. Кнопка

```c
if (encoderButtonPressed()) {
    soundOn = !soundOn;
    buzzerSetSound(soundOn);
}
```

Кожне натискання перемикає звук: увімкнено → вимкнено → увімкнено.

#### Крок 5. Пауза

```c
vTaskDelay(pdMS_TO_TICKS(5));
```

Задача віддає процесор на 5 мс. Без цього FreeRTOS вважатиме, що задача зависла, і спрацює task watchdog. На читання енкодера це не впливає: його читає таймер.

### 7. Мелодія: `buzzerPlay()`

```c
for (size_t i = 0; i < count; i++) {
    if (notes[i].hz == 0) {
        buzzerSetSound(false);              // пауза
    } else {
        buzzerSetFrequency(notes[i].hz);    // висота ноти
        buzzerSetSound(true);
    }
    vTaskDelay(pdMS_TO_TICKS(notes[i].ms)); // тривалість ноти
}

buzzerSetFrequency(backToHz);
buzzerSetSound(true);
```

Для кожної ноти:
1. Ставиться її частота, або вимикається звук, якщо це пауза.
2. Нота звучить `ms` мілісекунд.

Після останньої ноти базер повертається до звичайної частоти.

### 8. Антибрязкіт кнопки: `encoderButtonPressed()`

```c
if (read != lastRead) {
    lastRead = read;
    lastChangeUs = now;
    return false;
}

if (read != stableState && now - lastChangeUs >= DEBOUNCE_MS * 1000) {
    stableState = read;
    return read == 0;
}
```

1. Сигнал змінився — контакт ще стрибає. Перезапускаємо відлік і нічого не повертаємо.
2. Сигнал 30 мс не змінювався і відрізняється від підтвердженого стану — приймаємо новий стан.
3. Повертаємо `true` лише для натискання (`0`, бо кнопка з підтяжкою вгору), а не для відпускання.

### 9. Загальна картина

```
gptimer (кожна 1 мс, апаратно)
 └─ onTimer(): CLK+DT → таблиця MOVES → count ±1

app_main, цикл кожні 5 мс
 ├─ кліки змінились? → hz ±100 (200..4000) → LEDC
 │    └─ щойно дійшли до 200 Гц? → «Сакура, сакура»
 ├─ натиснули кнопку? → звук ON/OFF
 └─ vTaskDelay(5 мс)

LEDC (апаратно)
 └─ меандр 50 % потрібної частоти → GPIO18 → базер
```

Обидві ключові задачі виконує апаратура: таймер читає енкодер, LEDC генерує звук. Програма лише перераховує частоту, коли кількість кліків змінилася.

## Налаштування

Усі параметри в `include/Config.h`:

| Параметр | Значення | Що означає |
|---|---|---|
| `START_HZ` | 1000 | частота при старті |
| `STEP_HZ` | 100 | зміна частоти на один клік |
| `MIN_HZ`, `MAX_HZ` | 200 / 4000 | межі частоти, на `MIN_HZ` грає мелодія |
| `ENCODER_READ_US` | 1000 | як часто таймер читає енкодер, мкс |
| `COUNTS_PER_STEP` | 4 | змін сигналу на один клік |

- **Частота змінюється не в той бік:** поміняйте місцями CLK і DT.
- **Один клік дає 200 Гц замість 100:** поставте `COUNTS_PER_STEP 2`.

Мелодія записана в `src/main.c` як масив нот `{частота в Гц, тривалість у мс}`, де `0` означає паузу.

## Структура проєкту

```
platformio.ini         платформа espressif32@6.9.0, ESP-IDF
CMakeLists.txt         проєкт ESP-IDF
include/Config.h       піни і параметри
include/Encoder.h      API енкодера
include/Buzzer.h       API базера
src/CMakeLists.txt     збирає всі .c із src
src/Encoder.c          таймер, читання енкодера, кнопка
src/Buzzer.c           LEDC ШІМ, відтворення мелодії
src/main.c             зміна частоти, мелодія на мінімумі
```

## Збірка

1. Відкрити папку проєкту в VS Code з PlatformIO.
2. **Build**, потім **Upload**, потім **Monitor**.

## Результат

*(додайте відео роботи і скриншот монітора порту)*
