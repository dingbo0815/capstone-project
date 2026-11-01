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
#include <Adafruit_SSD1306.h>
#include "config.h"

// ---- 裝置 ----
DHT dht(PIN_DHT, DHT22);
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature ds(&oneWire);
INA226 ina(0x40);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);   // 0.96 吋 OLED；若是 128x32 請改成 32

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
float readNtc(float& volts, float& ohms) {
  volts = analogReadMilliVolts(PIN_NTC) / 1000.0f;
  if (volts <= 0.01f || volts >= 3.29f) { ohms = NAN; return NAN; }  // 開路或短路
  ohms = NTC_R_FIXED * volts / (3.3f - volts);                        // 10k 在上、NTC 在下
  float invT = 1.0f / 298.15f + logf(ohms / NTC_R0) / NTC_B;
  return 1.0f / invT - 273.15f;
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

  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  Serial.println(oledOk ? "[OLED] 初始化成功" : "[OLED] 初始化失敗：I2C 掃描應該要看到 0x3C");

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
  float vNtc, rNtc;
  float tNtc = readNtc(vNtc, rNtc);

  // INA226
  float vBus = inaOk ? ina.getBusVoltage() : NAN;
  float iBus = inaOk ? ina.getCurrent_mA() : NAN;

  // ---- 序列監控 ----
  Serial.printf("DHT22   : %6.2f °C  %5.1f %%\n", tDht, rh);
  for (int i = 0; i < dsCount && i < 3; i++)
    Serial.printf("DS18B20#%d: %6.2f °C\n", i, tDs[i]);
  Serial.printf("NTC     : %6.2f °C  (%.3f V, %.0f Ω)\n", tNtc, vNtc, rNtc);
  Serial.printf("INA226  : %6.3f V  %8.2f mA\n", vBus, iBus);
  Serial.println("--------------------------------");

  // ---- OLED ----
  if (oledOk) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 0);
    oled.printf("DHT %.1fC %.0f%%\n", tDht, rh);
    for (int i = 0; i < dsCount && i < 3; i++) oled.printf("DS%d %.2fC\n", i, tDs[i]);
    oled.printf("NTC %.1fC\n", tNtc);
    oled.printf("INA %.2fV %.1fmA\n", vBus, iBus);
    oled.display();
  }

  delay(2000);   // DHT22 最短讀取間隔 2 秒
}
