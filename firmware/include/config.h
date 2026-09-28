#pragma once
#define FW_VERSION        "0.1.0"

// ---- 腳位 ----
#define PIN_RPWM          25
#define PIN_LPWM          26
#define PIN_EN            27
#define PIN_DHT           4
#define PIN_ONEWIRE       18
#define PIN_FAN           19
#define PIN_NTC           34   // ADC1，Wi-Fi 開啟時仍可用

// ---- PWM ----
#define PWM_FREQ_HZ       20000
#define PWM_BITS          10

// ---- 任務週期 (ms) ----
#define PERIOD_SAFETY_MS  200
#define PERIOD_SENSOR_MS  2000
#define PERIOD_CONTROL_MS 2000
#define PERIOD_COMM_MS    1000
#define PERIOD_LOG_MS     2000

// ---- 安全門檻 ----
#define T_HOT_TRIP_C      65.0f
#define T_FAN_OFF_C       40.0f
#define V_BAT_MIN         9.9f
#define REVERSE_WAIT_MS   3000

// ---- 熱端 NTC ----
#define NTC_R_FIXED       10000.0f
#define NTC_R0            10000.0f
#define NTC_B             3950.0f