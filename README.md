# 智能保鮮罩（FreshCover）

明志科技大學電子工程系專題。以 ESP32 控制半導體致冷片（TEC）調節罩內溫濕度，並建立數位孿生模型，即時預測罩內溫度、電池續航與異常狀況。

## 資料夾結構

| 資料夾 | 內容 |
|---|---|
| `firmware/` | ESP32 韌體（PlatformIO） |
| `analysis/` | Python 資料記錄與分析 |
| `web/` | 3D 數位孿生網頁 |
| `logs/raw/` | 原始測試資料（不可修改） |
| `logs/processed/` | 分析結果與圖表 |
| `blender/exports/` | 3D 模型（GLB） |
| `docs/` | 規格文件，資料格式見 [csv_spec.md](docs/csv_spec.md) |

---

## 測試操作說明（組員用）

不需要會寫程式，照步驟操作即可。有問題先截圖再問。

### 第一次使用：環境設定（只需做一次）

1. 安裝 Python，安裝時**勾選「Add python.exe to PATH」**
2. 在專案資料夾 `capstone-project` 開啟 PowerShell，依序執行：

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r analysis/requirements.txt
```

若出現「已停用指令碼執行」，先執行下面這行再重試：

```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

### 每次測試的步驟

**測試前**

1. 確認 TEC 已關閉**至少 30 分鐘**（讓罩內溫度等於環境溫度）
2. 用 USB 線把 ESP32 接上筆電
3. 確認沒有其他程式開著序列埠（例如 VS Code 的 Serial Monitor）

**開始記錄**

在 `capstone-project` 資料夾開啟 PowerShell：

```powershell
.venv\Scripts\Activate.ps1
python analysis/serial_logger.py --test A --load none --operator 你的名字 --minutes 60
```

| 參數 | 填什麼 |
|---|---|
| `--test` | 測試代號，例如 A、B、C |
| `--load` | 罩內放的東西：沒放填 `none`，放 500g 水瓶填 `water_500g` |
| `--operator` | 你的名字 |
| `--minutes` | 記錄幾分鐘，時間到自動停止 |
| `--note` | 其他狀況（選填），例如 `--note "窗戶開著"` |

**記錄中**

- 畫面每分鐘會顯示「已記錄 N 列」，代表正常
- **開蓋時按一下標記鈕**（lid 欄位）
- 不要拔 USB 線、不要讓筆電進入睡眠
- 如果出現「間隔異常」或「格式錯誤」，記下時間，結束後回報

**結束後**

畫面會顯示摘要，確認「格式錯誤」與「間隔異常」都是 0，然後把摘要截圖傳到群組。資料會自動存在 `logs/raw/`，**不要修改或刪除**。

---

## 開發者

- 韌體：在 `firmware/` 內執行 `pio run -t upload`
- 序列監控：`pio device monitor`
- 資料管線驗證：`python analysis/verify_fake.py`
