// sensor_check.cpp — 階段 1 感測器檢查程式
// 依序檢查 I2C 裝置、DHT22、DS18B20、NTC、INA226、OLED
// 任何一個失敗都不會影響其他感測器，方便逐一排除問題
#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <INA226.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>   // OLED 驅動晶片已確認是 SH1106（不是 SSD1306）
#include "config.h"

// ---- 裝置 ----
DHT dht(PIN_DHT, DHT22);
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature ds(&oneWire);
INA226 ina(0x40);
Adafruit_SH1106G oled(128, 64, &Wire, -1);   // SH1106，128x64，沒有接 RESET 腳所以填 -1

bool inaOk = false;
bool oledOk = false;
int dsCount = 0;

// ---- I2C 掃描：列出所有回應的位址 ----
void scanI2C() {
  Serial.println("[I2C] 掃描中...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[I2C] 找到 0x%02X", addr);
      if (addr == 0x3C) Serial.print("  (OLED)");
      if (addr == 0x40) Serial.print("  (INA226)");
      Serial.println();
      found++;
    }
  }
  if (found == 0) Serial.println("[I2C] 一個都沒找到：檢查 SDA=21、SCL=22、電源");
}

// ---- 印出 DS18B20 的位址（用來辨識哪一顆是冷端、哪一顆是環境） ----
void printDsAddresses() {
  dsCount = ds.getDeviceCount();
  Serial.printf("[DS18B20] 找到 %d 顆\n", dsCount);
  if (dsCount == 0) Serial.printf("[DS18B20] 檢查 4.7k 上拉電阻、DATA=GPIO%d、配色\n", PIN_ONEWIRE);
  for (int i = 0; i < dsCount; i++) {
    DeviceAddress a;
    if (ds.getAddress(a, i)) {
      Serial.printf("[DS18B20] #%d 位址 ", i);
      for (int k = 0; k < 8; k++) Serial.printf("%02X", a[k]);
      Serial.println();
    }
  }
}

// ---- NTC：讀電壓 → 換算電阻 → B 參數方程式求溫度 ----
// 舊版用「電壓 >= 3.29V」判斷開路，但 ESP32 的 ADC 在 3.1V 左右就飽和，永遠讀不到 3.29V，
// 開路時會算出一個假的低溫（約 -20～-26°C），過溫保護等於失效。
// 新版改成「算出來的溫度超出合理範圍就視為故障」：
//   NTC 貼在熱端散熱片，正常運作不可能低於 -10°C 或高於 110°C
//   -10°C 對應 2.82V、110°C 對應 0.17V，兩個門檻都在 ADC 讀得準的範圍內
// 之後寫主程式時，把這兩個常數搬到 config.h
constexpr float NTC_T_MIN_C = -10.0f;
constexpr float NTC_T_MAX_C = 110.0f;
constexpr int   NTC_SAMPLES = 16;     // 多次取樣平均，降低 ADC 雜訊

enum NtcStatus { NTC_OK, NTC_OPEN, NTC_SHORT };

const char* ntcStatusText(NtcStatus s) {
  switch (s) {
    case NTC_OPEN:  return "開路（線斷了或沒插）";
    case NTC_SHORT: return "短路（或溫度超過上限）";
    default:        return "正常";
  }
}

// 回傳狀態；只有 NTC_OK 時 tempC 才是有效值，故障時 tempC = NAN
NtcStatus readNtc(float& tempC, float& volts, float& ohms) {
  uint32_t sum = 0;
  for (int i = 0; i < NTC_SAMPLES; i++) sum += analogReadMilliVolts(PIN_NTC);
  volts = sum / (float)NTC_SAMPLES / 1000.0f;
  tempC = NAN;
  ohms = NAN;

  // 先擋掉會讓公式出錯的極端值（分母接近 0、log(0)）
  if (volts >= 3.2f)  return NTC_OPEN;
  if (volts <= 0.02f) return NTC_SHORT;

  ohms = NTC_R_FIXED * volts / (3.3f - volts);   // 10k 在上、NTC 在下
  float invT = 1.0f / 298.15f + logf(ohms / NTC_R0) / NTC_B;
  float t = 1.0f / invT - 273.15f;

  if (t < NTC_T_MIN_C) return NTC_OPEN;    // 開路：電壓被 10k 拉高 → 算出來極冷
  if (t > NTC_T_MAX_C) return NTC_SHORT;   // 短路：電壓接近 0 → 算出來極熱
  tempC = t;
  return NTC_OK;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n===== 感測器檢查 =====");

  Wire.begin();          // SDA=21, SCL=22
  scanI2C();

  dht.begin();

  ds.begin();
  printDsAddresses();

  inaOk = ina.begin();
  if (inaOk) {
    // 目前是原廠 0.1Ω (R100)，量程約 0.8A。換成 R010 後改成 setMaxCurrentShunt(8.0, 0.01)
    ina.setMaxCurrentShunt(0.8, 0.1);
    Serial.println("[INA226] 初始化成功（分流電阻設定 0.1Ω）");
  } else {
    Serial.println("[INA226] 初始化失敗：I2C 掃描應該要看到 0x40");
  }

  oledOk = oled.begin(0x3C, true);   // 位址 0x3C；第二個參數 true = 初始化時重置顯示器
  if (oledOk) {
    oled.clearDisplay();             // SH1106 開機時顯示記憶體是亂的，不清掉就是滿版雜點
    oled.display();
  }
  Serial.println(oledOk ? "[OLED] SH1106 初始化成功" : "[OLED] 初始化失敗：I2C 掃描應該要看到 0x3C");

  Serial.println("======================\n");
}

void loop() {
  // DHT22
  float tDht = dht.readTemperature();
  float rh = dht.readHumidity();

  // DS18B20
  ds.requestTemperatures();
  float tDs[3] = {NAN, NAN, NAN};
  for (int i = 0; i < dsCount && i < 3; i++) tDs[i] = ds.getTempCByIndex(i);

  // NTC
  float tNtc, vNtc, rNtc;
  NtcStatus ntcSt = readNtc(tNtc, vNtc, rNtc);

  // INA226
  float vBus = inaOk ? ina.getBusVoltage() : NAN;
  float iBus = inaOk ? ina.getCurrent_mA() : NAN;

  // ---- 序列監控 ----
  Serial.printf("DHT22   : %6.2f °C  %5.1f %%\n", tDht, rh);
  for (int i = 0; i < dsCount && i < 3; i++)
    Serial.printf("DS18B20#%d: %6.2f °C\n", i, tDs[i]);
  if (ntcSt == NTC_OK)
    Serial.printf("NTC     : %6.2f °C  (%.3f V, %.0f Ω)\n", tNtc, vNtc, rNtc);
  else
    Serial.printf("NTC     : 故障 - %s  (%.3f V)\n", ntcStatusText(ntcSt), vNtc);
  Serial.printf("INA226  : %6.3f V  %8.2f mA\n", vBus, iBus);
  Serial.println("--------------------------------");

  // ---- OLED ----
  if (oledOk) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SH110X_WHITE);
    // 從第 2 行（y = 8）開始寫：這片螢幕頂端有幾條像素線不亮（玻璃有裂痕）
    // 最多 6 行（DHT、DS×3、NTC、INA）× 8 像素 = 48，從 8 開始到 56，還在 64 以內
    oled.setCursor(0, 8);
    oled.printf("DHT %.1fC %.0f%%\n", tDht, rh);
    for (int i = 0; i < dsCount && i < 3; i++) oled.printf("DS%d %.2fC\n", i, tDs[i]);
    if (ntcSt == NTC_OK) oled.printf("NTC %.1fC\n", tNtc);
    else oled.printf("NTC %s\n", ntcSt == NTC_OPEN ? "OPEN!" : "SHORT!");   // OLED 字型沒有中文
    oled.printf("INA %.2fV %.1fmA\n", vBus, iBus);
    oled.display();
  }

  delay(2000);   // DHT22 最短讀取間隔 2 秒
}