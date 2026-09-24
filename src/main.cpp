#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include <math.h> 

constexpr gpio_num_t BTN_PIN = GPIO_NUM_9;  // BOOT butonu
constexpr gpio_num_t LED_PIN = GPIO_NUM_4;  // Harici LED pini

// LEDC (PWM) Ayarları
constexpr ledc_mode_t LEDC_SPEED_MODE     = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t LEDC_TIMER_NUM     = LEDC_TIMER_0;
constexpr ledc_channel_t LEDC_CHANNEL_NUM = LEDC_CHANNEL_0;
constexpr uint32_t LEDC_DUTY_RES          = LEDC_TIMER_10_BIT; // 0 - 1023
constexpr uint32_t LEDC_MAX_DUTY          = 1023;

// GAMA PARAMETRELERİ
constexpr float GAMMA                     = 2.2f;
constexpr int TOTAL_STEPS                 = 50; 
constexpr int STEP_DELAY_MS               = 20;  

enum class LedState {
    Off,
    On
};

static volatile bool button_pressed = false;

// 1) KESME (INTERRUPT) FONKSİYONU
static void IRAM_ATTR isr_handler(void* /*arg*/) {
    button_pressed = true;
}

// 2) GAMA MATEMATİKSEL FORMÜLÜ

static uint32_t calculate_gamma_duty(int step) {
    float oran = (float)step / (float)TOTAL_STEPS; // 0.0 ile 1.0 arası normalize zaman
    float duzeltilmis_oran = powf(oran, GAMMA);   // (t / T)^2.2
    return (uint32_t)(duzeltilmis_oran * LEDC_MAX_DUTY);
}

extern "C" void app_main() {
    // LEDC TIMER KURULUMU
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode       = LEDC_SPEED_MODE;
    timer_cfg.duty_resolution  = static_cast<ledc_timer_bit_t>(LEDC_DUTY_RES);
    timer_cfg.timer_num        = LEDC_TIMER_NUM;
    timer_cfg.freq_hz          = 5000;
    timer_cfg.clk_cfg          = LEDC_AUTO_CLK;
    ledc_timer_config(&timer_cfg);

    // LEDC KANAL KURULUMU
    ledc_channel_config_t channel_cfg = {};
    channel_cfg.gpio_num   = LED_PIN;
    channel_cfg.speed_mode = LEDC_SPEED_MODE;
    channel_cfg.channel    = LEDC_CHANNEL_NUM;
    channel_cfg.intr_type  = LEDC_INTR_DISABLE;
    channel_cfg.timer_sel  = LEDC_TIMER_NUM;
    channel_cfg.duty       = 0;
    channel_cfg.hpoint     = 0;
    ledc_channel_config(&channel_cfg);

    // BUTON VE KESME KURULUMU
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

    // ANA DÖNGÜ
    while (true) {
        if (button_pressed) {
            button_pressed = false;

            // Debounce
            vTaskDelay(pdMS_TO_TICKS(200));

            durum = (durum == LedState::Off) ? LedState::On : LedState::Off;

            if (durum == LedState::On) {
               
                for (int step = 0; step <= TOTAL_STEPS; step++) {
                    uint32_t duty = calculate_gamma_duty(step);
                    ledc_set_duty(LEDC_SPEED_MODE, LEDC_CHANNEL_NUM, duty);
                    ledc_update_duty(LEDC_SPEED_MODE, LEDC_CHANNEL_NUM);
                    vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
                }
            } else {
                
                for (int step = TOTAL_STEPS; step >= 0; step--) {
                    uint32_t duty = calculate_gamma_duty(step);
                    ledc_set_duty(LEDC_SPEED_MODE, LEDC_CHANNEL_NUM, duty);
                    ledc_update_duty(LEDC_SPEED_MODE, LEDC_CHANNEL_NUM);
                    vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}