// 階段 0：五任務骨架
// 目的：驗證任務週期、堆疊用量，並輸出假資料測試資料管線
#include <Arduino.h>                     // Arduino 核心（Serial、millis、delay，並帶入 FreeRTOS API）
#include "config.h"                      // 專案設定：FW_VERSION、各任務週期 PERIOD_*_MS

// ---------- 序列埠互斥鎖 ----------
// 多個任務同時 printf 會讓輸出交錯成亂碼，所以印之前要先取得鎖
static SemaphoreHandle_t printMutex;     // 保護 Serial 輸出的互斥鎖 handle，於 setup() 建立

// ---------- 週期統計 ----------
 struct PeriodStat {                     // 每個任務一份的週期統計資料
  const char* name;                      // 任務名稱（印出時用來辨識）
  uint32_t reportEvery;          // 每執行幾次回報一次
  int64_t  last;                 // 上次執行時間 (us)
  int64_t  minDt;                // 這段期間最短間隔 (us)
  int64_t  maxDt;                // 這段期間最長間隔 (us)
  uint32_t n;                            // 累計執行次數

  // 用建構子設定初始值（C++11 相容）
  PeriodStat(const char* nm, uint32_t every)
    : name(nm), reportEvery(every), last(0), minDt(INT64_MAX), maxDt(0), n(0) {}  // minDt 設最大值，第一次比較必定被更新
};

static void tick(PeriodStat& s) {                // 每次任務執行時呼叫：量測間隔並定期回報
  int64_t now = esp_timer_get_time();          // 開機後的微秒數
  if (s.last != 0) {                             // 第一次執行沒有上一次可比，跳過
    int64_t dt = now - s.last;                   // 與上次執行的時間差 (us)
    if (dt < s.minDt) s.minDt = dt;              // 更新最短間隔
    if (dt > s.maxDt) s.maxDt = dt;              // 更新最長間隔
  }
  s.last = now;                                  // 記下本次時間，供下次計算

  if (++s.n % s.reportEvery == 0) {              // 次數加一，達到回報次數時印出統計
    xSemaphoreTake(printMutex, portMAX_DELAY);   // 取得序列埠鎖（無限等待）
    Serial.printf("[%-7s] n=%-5u dt_min=%8.3f ms  dt_max=%8.3f ms  core=%d  stack_free=%u B\n",  // 名稱、次數、最短/最長間隔、所在核心、堆疊剩餘
                  s.name, s.n, s.minDt / 1000.0, s.maxDt / 1000.0,   // us 轉 ms
                  xPortGetCoreID(), uxTaskGetStackHighWaterMark(NULL));  // 目前核心編號；本任務堆疊歷史最低剩餘量
    xSemaphoreGive(printMutex);                  // 釋放序列埠鎖
    s.minDt = INT64_MAX;                        // 重新統計下一段
    s.maxDt = 0;                                 // 最長間隔歸零
  }
}

// ---------- 通用任務本體 ----------
// 每 10 秒左右回報一次：reportEvery = 10000 / 週期
static void periodicTask(void* arg) {            // safety / sensor / control / comm 共用的任務函式
  PeriodStat* s = (PeriodStat*)arg;              // 取回建立任務時傳入的統計結構
  const TickType_t period = pdMS_TO_TICKS(10000 / s->reportEvery);  // 由 reportEvery 反推週期 (ms) 再轉成 tick
  TickType_t lastWake = xTaskGetTickCount();     // 記錄起始 tick，作為週期基準
  for (;;) {                                     // FreeRTOS 任務不可返回，必須無窮迴圈
    tick(*s);                                    // 量測並（定期）回報週期
    vTaskDelayUntil(&lastWake, period);         // 以「上次喚醒時間」為基準，週期不會漂移
  }
}

// ---------- 記錄任務：輸出假資料 ----------
static void logTask(void* arg) {                 // 記錄任務：除了量週期，還輸出 CSV 假資料
  PeriodStat* s = (PeriodStat*)arg;              // 取回統計結構
  TickType_t lastWake = xTaskGetTickCount();     // 週期基準 tick
  for (;;) {                                     // 任務主迴圈
    tick(*s);                                    // 量測週期
    uint32_t t = millis();                       // 開機後毫秒數，作為資料時間戳
    // 假溫度：25°C 為中心、振幅 5°C、週期 60 秒的正弦波
    float t_in = 25.0f + 5.0f * sinf(2.0f * PI * (t / 1000.0f) / 60.0f);

    xSemaphoreTake(printMutex, portMAX_DELAY);   // 取得序列埠鎖
    // 欄位：t_ms,mode,duty,t_in,rh_in,t_cold,t_hot,t_amb,v_bus,i_bus,p_bus,lid
    Serial.printf("D,%lu,1,1.000,%.2f,70.0,15.00,45.00,30.00,11.800,4.500,53.10,0\n",  // 開頭 D 表資料列；除 t_ms、t_in 外皆為固定假值
                  (unsigned long)t, t_in);       // 轉 unsigned long 以符合 %lu
    xSemaphoreGive(printMutex);                  // 釋放序列埠鎖

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(PERIOD_LOG_MS));  // 等到下一個記錄週期
  }
}

// ---------- 統計資料（名稱, 每幾次回報） ----------
static PeriodStat stSafety  {"safety",  10000 / PERIOD_SAFETY_MS};   // 安全監控任務的統計
static PeriodStat stSensor  {"sensor",  10000 / PERIOD_SENSOR_MS};   // 感測器讀取任務的統計
static PeriodStat stControl {"control", 10000 / PERIOD_CONTROL_MS};  // 控制運算任務的統計
static PeriodStat stComm    {"comm",    10000 / PERIOD_COMM_MS};     // 通訊任務的統計
static PeriodStat stLog     {"log",     10000 / PERIOD_LOG_MS};      // 記錄任務的統計

void setup() {                                   // 開機執行一次
  Serial.begin(115200);                          // 開啟序列埠，鮑率 115200
  delay(500);                                    // 等序列埠 / 監視器就緒，避免開頭訊息遺失
  printMutex = xSemaphoreCreateMutex();          // 建立互斥鎖（必須在任何任務開始印之前）
  Serial.printf("\n=== FreshCover FW %s ===\n", FW_VERSION);  // 印出韌體版本

  //                      函式          名稱       堆疊(B) 參數        優先權 handle  核心
  xTaskCreatePinnedToCore(periodicTask, "safety",  4096, &stSafety,  5, NULL, 1);  // 安全任務：最高優先權，核心 1
  xTaskCreatePinnedToCore(periodicTask, "sensor",  4096, &stSensor,  4, NULL, 1);  // 感測任務：核心 1
  xTaskCreatePinnedToCore(periodicTask, "control", 4096, &stControl, 3, NULL, 1);  // 控制任務：核心 1
  xTaskCreatePinnedToCore(periodicTask, "comm",    4096, &stComm,    2, NULL, 0);  // 通訊任務：核心 0（與 WiFi/BT 同核）
  xTaskCreatePinnedToCore(logTask,      "log",     4096, &stLog,     1, NULL, 0);  // 記錄任務：最低優先權，核心 0
}

void loop() {                                    // Arduino 主迴圈（本專案不使用）
  vTaskDelete(NULL);   // 所有工作都在任務裡，Arduino 的 loop 任務用不到，直接刪掉釋放記憶體
}
