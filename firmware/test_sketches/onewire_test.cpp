// onewire_test.cpp — 只測 OneWire 匯流排上有沒有裝置回應
#include <Arduino.h>
#include <OneWire.h>
#include "config.h"

OneWire ow(PIN_ONEWIRE);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.printf("\n===== OneWire 測試，腳位 GPIO %d =====\n", PIN_ONEWIRE);
}

void loop() {
  // 1. 閒置電壓：上拉正常的話應該讀到 1
  pinMode(PIN_ONEWIRE, INPUT);
  Serial.printf("閒置電位: %d（應為 1）\n", digitalRead(PIN_ONEWIRE));

  // 2. 重置脈衝：有任何 DS18B20 回應就會回傳 1
  uint8_t presence = ow.reset();
  Serial.printf("重置回應: %s\n", presence ? "有裝置回應" : "沒有任何裝置回應");

  // 3. 搜尋位址
  uint8_t addr[8];
  int n = 0;
  ow.reset_search();
  while (ow.search(addr)) {
    Serial.print("  找到位址: ");
    for (int i = 0; i < 8; i++) Serial.printf("%02X", addr[i]);
    Serial.println(OneWire::crc8(addr, 7) == addr[7] ? "  (CRC 正確)" : "  (CRC 錯誤)");
    n++;
  }
  Serial.printf("共 %d 顆\n------------\n", n);
  delay(2000);
}
