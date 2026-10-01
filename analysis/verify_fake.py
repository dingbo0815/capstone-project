"""
verify_fake.py
用已知的正弦波假資料驗證整條資料管線（ESP32 → 序列埠 → Python → CSV）。

用法（在 capstone-project 根目錄執行）：
    python analysis/verify_fake.py                          # 自動讀取最新的 fake 檔案
    python analysis/verify_fake.py logs/raw/20260928_fake_1.csv
"""
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

ROOT = Path(__file__).resolve().parent.parent
RAW_DIR = ROOT / "logs" / "raw"
OUT_DIR = ROOT / "logs" / "processed"

PERIOD_MS = 2000
ERR_LIMIT = 0.006   # 韌體以 %.2f 輸出，四捨五入誤差最多 0.005°C


def main():
    if len(sys.argv) > 1:
        path = Path(sys.argv[1])
    else:
        files = sorted(RAW_DIR.glob("*_fake_*.csv"), key=lambda p: p.stat().st_mtime)
        if not files:
            sys.exit("找不到 logs/raw/*_fake_*.csv，請先執行 serial_logger.py --test fake")
        path = files[-1]

    df = pd.read_csv(path, comment="#")
    t = df["t_ms"] / 1000.0

    # 韌體產生假資料用的同一條公式
    expected = 25.0 + 5.0 * np.sin(2 * np.pi * t / 60.0)
    err = (df["t_in"] - expected).abs()
    dt = df["t_ms"].diff().dropna()

    # ---- 三項檢查 ----
    ok_rows = len(df) >= 30                          # 至少 1 分鐘的資料
    ok_err = err.max() <= ERR_LIMIT                  # 數值正確
    ok_dt = dt.between(PERIOD_MS - 10, PERIOD_MS + 10).all()   # 沒有漏行

    print(f"檔案：{path.name}")
    print(f"資料列數：{len(df)}  {'✓' if ok_rows else '✗ 少於 30 列'}")
    print(f"最大數值誤差：{err.max():.4f} °C  {'✓' if ok_err else '✗ 超過 0.006'}")
    print(f"取樣間隔：{dt.min():.0f} ~ {dt.max():.0f} ms  {'✓' if ok_dt else '✗ 有漏行或間隔異常'}")
    print("\n結果：" + ("通過，資料管線正常" if (ok_rows and ok_err and ok_dt) else "未通過"))

    # ---- 圖表 ----
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(9, 6), sharex=True)
    ax1.plot(t, expected, "-", lw=1, label="Expected (formula)")
    ax1.plot(t, df["t_in"], "o", ms=3, label="Logged")
    ax1.set_ylabel("t_in (°C)")
    ax1.legend()
    ax1.set_title(path.name)

    ax2.plot(t, df["t_in"] - expected, ".-", ms=3)
    ax2.axhline(ERR_LIMIT, ls="--", lw=0.8)
    ax2.axhline(-ERR_LIMIT, ls="--", lw=0.8)
    ax2.set_ylabel("Error (°C)")
    ax2.set_xlabel("Time (s)")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    png = OUT_DIR / f"{path.stem}_check.png"
    fig.tight_layout()
    fig.savefig(png, dpi=120)
    print(f"圖表已存：{png.relative_to(ROOT)}")
    plt.show()


if __name__ == "__main__":
    main()
