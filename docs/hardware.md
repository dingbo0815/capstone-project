# 硬體規格

## ESP32 開發板 A（專題使用）
- MAC：20:43:a8:6d:28:04
- 晶片：ESP32-D0WD-V3 (revision v3.1)，雙核心 240MHz
- Flash：4MB
- 晶振：40MHz
- USB 轉 UART：CP2102

## ESP32 開發板 B（備用，有問題）
- 能執行程式，但 esptool 無法連線
- 序列監控需加 `--dtr 0 --rts 0` 才有輸出，推測自動重置電路異常
- 待試：EN 與 GND 之間加 10µF 電容