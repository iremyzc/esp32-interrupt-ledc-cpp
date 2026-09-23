#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

constexpr gpio_num_t BTN_PIN = GPIO_NUM_9;  // BOOT butonu
constexpr gpio_num_t LED_PIN = GPIO_NUM_4;  // Harici LED pini

// LEDC (PWM) Ayarları
constexpr ledc_mode_t LEDC_SPEED_MODE     = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t LEDC_TIMER_NUM     = LEDC_TIMER_0;
constexpr ledc_channel_t LEDC_CHANNEL_NUM = LEDC_CHANNEL_0;
constexpr uint32_t LEDC_DUTY_RES          = LEDC_TIMER_10_BIT; // 10-bit çözünürlük (0 - 1023)
constexpr uint32_t LEDC_MAX_DUTY          = 1023;              // %100 Parlaklık
constexpr int FADE_TIME_MS                = 1000;              // 1 saniyede açıl/kapan

enum class LedState {
    Off,
    On
};

static volatile bool button_pressed = false;

// 1) KESME (INTERRUPT) FONKSİYONU
static void IRAM_ATTR isr_handler(void* /*arg*/) {
    button_pressed = true;
}

extern "C" void app_main() {
    // 2) LEDC TIMER KURULUMU (Eksik alan hatasını önlemek için = {} ile sıfırlandı)
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode       = LEDC_SPEED_MODE;
    timer_cfg.duty_resolution  = static_cast<ledc_timer_bit_t>(LEDC_DUTY_RES);
    timer_cfg.timer_num        = LEDC_TIMER_NUM;
    timer_cfg.freq_hz          = 5000; // 5 kHz PWM frekansı
    timer_cfg.clk_cfg          = LEDC_AUTO_CLK;

    ledc_timer_config(&timer_cfg);

    // LEDC KANAL KURULUMU
    ledc_channel_config_t channel_cfg = {};
    channel_cfg.gpio_num   = LED_PIN;
    channel_cfg.speed_mode = LEDC_SPEED_MODE;
    channel_cfg.channel    = LEDC_CHANNEL_NUM;
    channel_cfg.intr_type  = LEDC_INTR_DISABLE;
    channel_cfg.timer_sel  = LEDC_TIMER_NUM;
    channel_cfg.duty       = 0; // Başlangıçta kapalı
    channel_cfg.hpoint     = 0;

    ledc_channel_config(&channel_cfg);

    // Donanımsal fade servisini başlat
    ledc_fade_func_install(0);

    // 3) BUTON VE KESME KURULUMU
    gpio_config_t btn_cfg = {};
    btn_cfg.pin_bit_mask = (1ULL << BTN_PIN);
    btn_cfg.mode         = GPIO_MODE_INPUT;
    btn_cfg.pull_up_en   = GPIO_PULLUP_ENABLE;
    btn_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn_cfg.intr_type    = GPIO_INTR_NEGEDGE;

    gpio_config(&btn_cfg);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_PIN, isr_handler, nullptr);

    auto durum = LedState::Off;

    // 4) DÖNGÜ
    while (true) {
        if (button_pressed) {
            button_pressed = false;

            // Debounce (buton arkı önleme)
            vTaskDelay(pdMS_TO_TICKS(200));

            durum = (durum == LedState::Off) ? LedState::On : LedState::Off;

            if (durum == LedState::On) {
                // Fade In: 0'dan 1023'e donanımsal geçiş
                ledc_set_fade_with_time(LEDC_SPEED_MODE, LEDC_CHANNEL_NUM, LEDC_MAX_DUTY, FADE_TIME_MS);
                ledc_fade_start(LEDC_SPEED_MODE, LEDC_CHANNEL_NUM, LEDC_FADE_NO_WAIT);
            } else {
                // Fade Out: 1023'ten 0'a donanımsal geçiş
                ledc_set_fade_with_time(LEDC_SPEED_MODE, LEDC_CHANNEL_NUM, 0, FADE_TIME_MS);
                ledc_fade_start(LEDC_SPEED_MODE, LEDC_CHANNEL_NUM, LEDC_FADE_NO_WAIT);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}