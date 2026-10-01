"""
serial_logger.py
從 ESP32 序列埠擷取以「D,」開頭的資料行，加上檔頭後存成 CSV。
其他行（除錯訊息）照常顯示在畫面上，但不寫入檔案。

用法（在 capstone-project 根目錄執行）：
    python analysis/serial_logger.py --test fake --minutes 3
    python analysis/serial_logger.py --test A --load none --operator 陳定波
"""
import argparse
import re
import time
from datetime import datetime
from pathlib import Path

import serial

ROOT = Path(__file__).resolve().parent.parent
RAW_DIR = ROOT / "logs" / "raw"
CONFIG_H = ROOT / "firmware" / "include" / "config.h"

COLUMNS = "t_ms,mode,duty,t_in,rh_in,t_cold,t_hot,t_amb,v_bus,i_bus,p_bus,lid"
N_FIELDS = len(COLUMNS.split(","))


def read_fw_version() -> str:
    """從 config.h 讀取 FW_VERSION（假設板子上燒的就是 repo 目前的版本）"""
    try:
        text = CONFIG_H.read_text(encoding="utf-8")
        m = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', text)
        return m.group(1) if m else "unknown"
    except OSError:
        return "unknown"


def next_path(test: str) -> Path:
    """依檔名規則 YYYYMMDD_<測試代號>_<第幾次>.csv 自動編號，不覆蓋舊檔"""
    RAW_DIR.mkdir(parents=True, exist_ok=True)
    date = datetime.now().strftime("%Y%m%d")
    run = 1
    while (RAW_DIR / f"{date}_{test}_{run}.csv").exists():
        run += 1
    return RAW_DIR / f"{date}_{test}_{run}.csv"


def main():
    ap = argparse.ArgumentParser(description="ESP32 資料記錄")
    ap.add_argument("--port", default="COM3")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--test", required=True, help="測試代號，例如 fake、A、B")
    ap.add_argument("--load", default="none", help="罩內負載，例如 none、water_500g")
    ap.add_argument("--location", default="")
    ap.add_argument("--operator", default="")
    ap.add_argument("--note", default="")
    ap.add_argument("--minutes", type=float, default=0, help="幾分鐘後自動停止；0 = 按 Ctrl+C 停止")
    ap.add_argument("--period-ms", type=int, default=2000, help="預期取樣週期，用來偵測漏行")
    args = ap.parse_args()

    path = next_path(args.test)

    # 開啟序列埠時把 DTR/RTS 設為 False，避免重置 ESP32（長時間測試中途不能被重置）
    ser = serial.Serial()
    ser.port = args.port
    ser.baudrate = args.baud
    ser.timeout = 1
    ser.dtr = False
    ser.rts = False
    ser.open()

    rows = bad = gaps = 0
    last_t = None
    start = time.time()

    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(f"# test_id: {args.test}\n")
        f.write(f"# fw_version: {read_fw_version()}\n")
        f.write(f"# date: {datetime.now():%Y-%m-%d %H:%M}\n")
        f.write(f"# load: {args.load}\n")
        f.write(f"# location: {args.location}\n")
        f.write(f"# operator: {args.operator}\n")
        f.write(f"# note: {args.note}\n")
        f.write(COLUMNS + "\n")

        print(f"記錄中 → {path.relative_to(ROOT)}")
        print("按 Ctrl+C 結束\n")

        try:
            while True:
                if args.minutes and time.time() - start >= args.minutes * 60:
                    break

                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").strip()

                # 非資料行：顯示但不存
                if not line.startswith("D,"):
                    if line:
                        print(f"  {line}")
                    continue

                # 檢查欄位數與時間戳記
                fields = line[2:].split(",")
                try:
                    if len(fields) != N_FIELDS:
                        raise ValueError
                    t_ms = int(fields[0])
                except ValueError:
                    bad += 1
                    print(f"  [格式錯誤] {line}")
                    continue

                # 間隔偏離預期超過 10% → 可能漏行或板子被重置
                if last_t is not None and abs(t_ms - last_t - args.period_ms) > args.period_ms * 0.1:
                    gaps += 1
                    print(f"  [間隔異常] {last_t} → {t_ms} ms")
                last_t = t_ms

                f.write(",".join(fields) + "\n")
                f.flush()   # 每列立刻寫入硬碟，程式意外中斷也不會遺失
                rows += 1
                if rows % 30 == 0:
                    print(f"  已記錄 {rows} 列（約 {rows * args.period_ms / 60000:.1f} 分鐘）")

        except KeyboardInterrupt:
            pass
        finally:
            ser.close()

    elapsed = time.time() - start
    print("\n===== 記錄結束 =====")
    print(f"檔案：{path.relative_to(ROOT)}")
    print(f"時間：{elapsed / 60:.1f} 分鐘")
    print(f"資料列：{rows}")
    print(f"格式錯誤：{bad}")
    print(f"間隔異常：{gaps}")


if __name__ == "__main__":
    main()
