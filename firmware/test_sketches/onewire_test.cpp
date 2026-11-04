// onewire_test.cpp — OneWire 匯流排逐步診斷
// 1. 閒置電位  2. 重置回應  3. 搜尋位址
// 4. 供電模式（VDD 有沒有真的接上）  5. 實際轉換一次並讀回溫度（含 CRC 檢查）
#include <Arduino.h>
#include <OneWire.h>
#include "config.h"

OneWire ow(PIN_ONEWIRE);

const int MAX_DEV = 8;
uint8_t addrs[MAX_DEV][8];

void printAddr(const uint8_t* a) {
  for (int i = 0; i < 8; i++) Serial.printf("%02X", a[i]);
}

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
  int n = 0;
  uint8_t a[8];
  ow.reset_search();
  while (n < MAX_DEV && ow.search(a)) {
    Serial.print("  找到位址: ");
    printAddr(a);
    Serial.println(OneWire::crc8(a, 7) == a[7] ? "  (CRC 正確)" : "  (CRC 錯誤)");
    memcpy(addrs[n++], a, 8);
  }
  Serial.printf("共 %d 顆\n", n);

  if (n > 0) {
    // 4. 供電模式：指令 0xB4（READ POWER SUPPLY）
    //    VDD 有接好的探頭回 1；只要有一顆 VDD 沒接（寄生供電），就會把線拉低回 0
    ow.reset();
    ow.skip();
    ow.write(0xB4);
    bool external = ow.read_bit();
    Serial.printf("供電模式: %s\n", external ? "外部供電（VDD 有接好）"
                                            : "寄生供電（至少一顆的 VDD 沒接到 3.3V！）");

    // 5. 所有探頭同時轉換溫度（0x44），12-bit 解析度最久要 750ms
    ow.reset();
    ow.skip();
    ow.write(0x44, 1);   // 第二個參數 1 = 寫完後保持高電位，給寄生供電的探頭用
    delay(800);
    ow.depower();

    // 逐顆讀回暫存器（0xBE），共 9 bytes，最後一個 byte 是 CRC
    for (int i = 0; i < n; i++) {
      uint8_t d[9];
      ow.reset();
      ow.select(addrs[i]);
      ow.write(0xBE);
      for (int k = 0; k < 9; k++) d[k] = ow.read();

      bool allFF = true;
      for (int k = 0; k < 9; k++) if (d[k] != 0xFF) allFF = false;
      bool crcOk = OneWire::crc8(d, 8) == d[8];
      float t = (int16_t)((d[1] << 8) | d[0]) / 16.0f;

      Serial.printf("  #%d ", i);
      printAddr(addrs[i]);
      if (allFF)           Serial.println("  讀回全部 0xFF：探頭沒有回應（接觸不良或沒電）");
      else if (!crcOk)     Serial.println("  CRC 錯誤：訊號受干擾（線太長或上拉太弱）");
      else if (t == 85.0f) Serial.println("  85.00 °C：開機預設值，代表轉換沒有執行（多半是供電不足）");
      else                 Serial.printf("  %.2f °C\n", t);
    }
  }

  Serial.println("------------");
  delay(2000);
}