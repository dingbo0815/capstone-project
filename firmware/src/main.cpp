// 階段 0：五任務骨架
// 目的：驗證任務週期、堆疊用量，並輸出假資料測試資料管線
#include <Arduino.h>
#include "config.h"
 
// ---------- 序列埠互斥鎖 ----------
// 多個任務同時 printf 會讓輸出交錯成亂碼，所以印之前要先取得鎖
static SemaphoreHandle_t printMutex;
 
// ---------- 週期統計 ----------
 struct PeriodStat {
  const char* name;
  uint32_t reportEvery;          // 每執行幾次回報一次
  int64_t  last;                 // 上次執行時間 (us)
  int64_t  minDt;                // 這段期間最短間隔 (us)
  int64_t  maxDt;                // 這段期間最長間隔 (us)
  uint32_t n;

  // 用建構子設定初始值（C++11 相容）
  PeriodStat(const char* nm, uint32_t every)
    : name(nm), reportEvery(every), last(0), minDt(INT64_MAX), maxDt(0), n(0) {}
};
 
static void tick(PeriodStat& s) {
  int64_t now = esp_timer_get_time();          // 開機後的微秒數
  if (s.last != 0) {
    int64_t dt = now - s.last;
    if (dt < s.minDt) s.minDt = dt;
    if (dt > s.maxDt) s.maxDt = dt;
  }
  s.last = now;
 
  if (++s.n % s.reportEvery == 0) {
    xSemaphoreTake(printMutex, portMAX_DELAY);
    Serial.printf("[%-7s] n=%-5u dt_min=%8.3f ms  dt_max=%8.3f ms  core=%d  stack_free=%u B\n",
                  s.name, s.n, s.minDt / 1000.0, s.maxDt / 1000.0,
                  xPortGetCoreID(), uxTaskGetStackHighWaterMark(NULL));
    xSemaphoreGive(printMutex);
    s.minDt = INT64_MAX;                        // 重新統計下一段
    s.maxDt = 0;
  }
}
 
// ---------- 通用任務本體 ----------
// 每 10 秒左右回報一次：reportEvery = 10000 / 週期
static void periodicTask(void* arg) {
  PeriodStat* s = (PeriodStat*)arg;
  const TickType_t period = pdMS_TO_TICKS(10000 / s->reportEvery);
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    tick(*s);
    vTaskDelayUntil(&lastWake, period);         // 以「上次喚醒時間」為基準，週期不會漂移
  }
}
 
// ---------- 記錄任務：輸出假資料 ----------
static void logTask(void* arg) {
  PeriodStat* s = (PeriodStat*)arg;
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    tick(*s);
    uint32_t t = millis();
    // 假溫度：25°C 為中心、振幅 5°C、週期 60 秒的正弦波
    float t_in = 25.0f + 5.0f * sinf(2.0f * PI * (t / 1000.0f) / 60.0f);
 
    xSemaphoreTake(printMutex, portMAX_DELAY);
    // 欄位：t_ms,mode,duty,t_in,rh_in,t_cold,t_hot,t_amb,v_bus,i_bus,p_bus,lid
    Serial.printf("D,%lu,1,1.000,%.2f,70.0,15.00,45.00,30.00,11.800,4.500,53.10,0\n",
                  (unsigned long)t, t_in);
    xSemaphoreGive(printMutex);
 
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(PERIOD_LOG_MS));
  }
}
 
// ---------- 統計資料（名稱, 每幾次回報） ----------
static PeriodStat stSafety  {"safety",  10000 / PERIOD_SAFETY_MS};
static PeriodStat stSensor  {"sensor",  10000 / PERIOD_SENSOR_MS};
static PeriodStat stControl {"control", 10000 / PERIOD_CONTROL_MS};
static PeriodStat stComm    {"comm",    10000 / PERIOD_COMM_MS};
static PeriodStat stLog     {"log",     10000 / PERIOD_LOG_MS};
 
void setup() {
  Serial.begin(115200);
  delay(500);
  printMutex = xSemaphoreCreateMutex();
  Serial.printf("\n=== FreshCover FW %s ===\n", FW_VERSION);
 
  //                      函式          名稱       堆疊(B) 參數        優先權 handle  核心
  xTaskCreatePinnedToCore(periodicTask, "safety",  4096, &stSafety,  5, NULL, 1);
  xTaskCreatePinnedToCore(periodicTask, "sensor",  4096, &stSensor,  4, NULL, 1);
  xTaskCreatePinnedToCore(periodicTask, "control", 4096, &stControl, 3, NULL, 1);
  xTaskCreatePinnedToCore(periodicTask, "comm",    4096, &stComm,    2, NULL, 0);
  xTaskCreatePinnedToCore(logTask,      "log",     4096, &stLog,     1, NULL, 0);
}
 
void loop() {
  vTaskDelete(NULL);   // 所有工作都在任務裡，Arduino 的 loop 任務用不到，直接刪掉釋放記憶體
}
 