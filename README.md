# Bible Verse Viewer — Flipper Zero App


A full-featured offline and online Bible reader for the Flipper Zero. Browse, search, and bookmark verses from SD card files, or look up any reference live via the Bible API — no account or API key required.



<img width="512" height="256" alt="Screenshot-20260222-151511" src="https://github.com/user-attachments/assets/1410d733-d8f0-46de-bb21-350bcc4c13e4" /> 

<img width="512" height="256" alt="Screenshot-20260222-151545" src="https://github.com/user-attachments/assets/4c988859-72fc-448d-b147-f50ae8b99315" /> 

<img width="512" height="256" alt="Screenshot-20260222-151838" src="https://github.com/user-attachments/assets/c5b08212-5fe7-4c64-b9cb-dc9aea7fe28b" /> 

<img width="512" height="256" alt="Screenshot-20260222-152806" src="https://github.com/user-attachments/assets/b6412c9a-4f99-42de-9dd0-f7eb0d9b20e5" /> 

<img width="512" height="256" alt="Screenshot-20260222-151731" src="https://github.com/user-attachments/assets/56577e86-331c-4122-97f7-f6c116447849" />




**Credits:** I used FlipperHTTP and Custom Font files from [JBlanked's](https://github.com/jblanked) project here... [Hello-World-Flipper-Zero](https://github.com/jblanked/Hello-World-Flipper-Zero).

This app was built with the assistance of [Claude](https://www.google.com/aclk?sa=L&pf=1&ai=DChsSEwj2ouHcyO6SAxVpFK0GHeImMJIYACICCAEQARoCcHY&co=1&ase=2&gclid=Cj0KCQiA7-rMBhCFARIsAKnLKtCsQdQJL2gmzqz5jsZBJ7hhI2l-HVuQ3-vNUiLQEz--k6bTNDZTz1EaArLgEALw_wcB&cid=CAASWuRop1HLx_wJXV_6YNoiswCR_CMK1oeLWmejtkjGwuQatm9-O6-DLRbSDIJ5DeArBQzVyEnGCyjfMfuNvLcrhULbSAVmO0jon8KWGZYhFK9_yxti0CDO2Z4tfA&cce=2&category=acrcp_v1_32&sig=AOD64_0rqbc12ct-DZqYYuzELr-Tdhr7gA&q&nis=4&adurl=https://chaton.ai/claude/?utm_source%3Dgoogle%26utm_medium%3Dcpc%26utm_campaign%3DGA%2520%7C%2520ChatOn%2520%7C%2520Web%2520%7C%2520US%2520%7C%2520Search%2520%7C%2520Main%2520%7C%2520CPA%2520%7C%252021.07.25%26utm_content%3D790508279447%26utm_term%3Dclaude%2520ai%26campaign_id%3D22809916767%26adset_id%3D186149762341%26ad_id%3D790508279447%26gad_source%3D1%26gad_campaignid%3D22809916767%26gbraid%3D0AAAAA9SXzF5Rcu5MIgUy86oFRsGg2F56b%26gclid%3DCj0KCQiA7-rMBhCFARIsAKnLKtCsQdQJL2gmzqz5jsZBJ7hhI2l-HVuQ3-vNUiLQEz--k6bTNDZTz1EaArLgEALw_wcB&ved=2ahUKEwjy_9rcyO6SAxUGOjQIHcy8ItMQ0Qx6BAgoEAE)

Image to XBM converter: [ImageToXBM](https://www.online-utility.org/image/convert/to/XBM)



**Version:** 1.3  
**Screen:** 128×64 px  
**Requires:** SD card with at least one verse file (`.txt` format)

---

## Features

### Offline (no hardware add-on needed)

| Feature | Description |
|---|---|
| **Browse** | Scroll through all verses in the loaded file |
| **Search** | Full-text keyword search across all verses |
| **Random Verse** | Picks a random verse on demand |
| **Verse of the Day** | One random verse chosen per day; persisted to SD so it stays consistent across restarts |
| **Bookmarks** | Long-press OK on any verse to toggle a bookmark; browse all bookmarks from the main menu |
| **5 Font Sizes** | Tiny (4×6), Small (5×8), Medium (6×10), Large (9×15), Flipper built-in |

### Online (requires  WiFi dev board flashed with [FlipperHTTP firmware](https://github.com/jblanked/FlipperHTTP))

Connects to [bible-api.com](https://bible-api.com) — free, no account or API key needed.

| Feature | Description |
|---|---|
| **Keyboard Lookup** | Type any reference (e.g. `john 3:16`) using the on-screen keyboard |
| **Book name autocomplete** | Ghost suggestion appears as you type; hold OK to accept the full book name including multi-word names like `1 Kings` or `Song of Solomon` |
| **Quick Picker** | Cycle Book, Chapter, and Verse with Left/Right; chapter and verse counts are clamped to real KJV values (1,189 chapters, up to 176 verses) |
| **9 Translations** | World English (WEB), King James (KJV), American Standard (ASV), Basic English (BBE), Darby, Douay-Rheims (DRA), Young's Literal (YLT), WEB British (WEBBE), Open English US (OEB-US) |
| **WiFi Status** | Board detection via PING/PONG on menu entry; live SSID and IP address display; WiFi icon in the API menu header shows connected (arc) or disconnected (X) |
| **Persistent state** | Last-used Book, Chapter, Verse, and Translation saved to SD and restored on next launch |

---

## Controls

| Button | Action |
|---|---|
| **Up / Down** | Navigate menus; scroll verse text and search results |
| **Left / Right** | Cycle Book / Chapter / Verse in the quick picker; switch settings sections |
| **OK (short)** | Select menu item; type character on keyboard; confirm action |
| **OK (long)** | Accept book name suggestion on keyboard; toggle caps lock; bookmark the current verse |
| **Back (short)** | Return to previous screen; backspace in text input |

---

## SD Card Setup

Place verse files in `/ext/apps_data/bible_viewer/`. The app auto-discovers any file matching `verses_*.txt` in that directory.

### Verse file format

Plain text, one verse per line, pipe-delimited:

```
Reference|Book|Verse text
```

Example:
```
John 3:16|John|For God so loved the world, that he gave his only begotten Son...
Gen 1:1|Genesis|In the beginning God created the heaven and the earth.
```

### Included translations

| File | Translation |
|---|---|
| `verses_en.txt` | King James Version (English) |
| `verses_esv.txt` | English Standard Version |
| `verses_de.txt` | Luther Bible 1912 (German) |

Any additional `verses_*.txt` file copied to the same folder will appear automatically in Settings.

### Settings file

`settings.txt` is written automatically to the same directory and stores all persistent preferences:

```
verse_file=verses_en.txt
font_size=1
api_trans=kjv
api_book=42
api_chapter=3
api_verse=16
daily_idx=8472
daily_day=9
```

Delete `settings.txt` to reset all preferences to defaults.

---

## Bible API Menu

```
┌─ Bible API ──────────────[~]┐
│ Lookup Verse                 │  OK → opens keyboard
│ < Book: John               > │  Left/Right cycles all 66 books
│ < Chapter: 3               > │  Left/Right cycles 1..N chapters
│ < Verse: 16                > │  Left/Right cycles 1..M verses
│ Trans: King James            │  OK → translation picker
│ WiFi Status                  │  OK → board/SSID/IP info
│ Back                         │
└──────────────────────────────┘
```

The WiFi icon `[~]` in the top-right corner of the header shows a WiFi arc symbol when the board is connected, or a white `X` when it is not. Board presence is detected automatically via PING/PONG when you enter the menu.

Pressing OK on any picker row (Book/Chapter/Verse) immediately fetches that reference. Arrow hints (`<` / `>`) appear on the selected row to indicate Left/Right is active.

### Keyboard Lookup — book name autocomplete

As you type a book name, a ghost suggestion appears right-aligned in the text field. Hold OK (long-press) while on any letter key to accept the suggestion and advance to the chapter number. Works for all 66 books including multi-word names:

```
Typed: "1 k"  →  suggestion: "1 Kings"
Typed: "so"   →  suggestion: "Song of Solomon"
Typed: "rev"  →  suggestion: "Revelation"
```

After accepting, type the chapter and verse normally, e.g. `1 Kings 8:22`.

### WiFi Status screen

| Field | Description |
|---|---|
| **Board** | `Found` if a FlipperHTTP board responded to PING; `Not found` otherwise |
| **State** | `Connected`, `Active` (mid-request), `Error`, or `Disconnected` |
| **SSID** | Name of the WiFi network the board is connected to |
| **IP** | IP address assigned to the board |

SSID and IP are fetched automatically when you open the Bible API menu, so the WiFi Status screen is always up to date without an extra round-trip.

---

## Hardware Requirements

### Offline mode
- Flipper Zero with SD card

### Online mode
- Flipper Zero with SD card
- ESP32-based WiFi dev board with [FlipperHTTP firmware](https://github.com/jblanked/FlipperHTTP)

Without the WiFi board the app detects the missing board via PING timeout and shows `X` in the header. All offline features work normally regardless.

---

## Building

The recommended way to compile a `.fap` is with **uFBT** (micro Flipper Build Tool). It downloads a pre-built SDK and toolchain automatically — no need to clone the full firmware.

### 1. Install uFBT

```bash
# Linux / macOS
python3 -m pip install --upgrade ufbt

# Windows
py -m pip install --upgrade ufbt
```

Requires Python 3.8 or newer.

### 2. Match the SDK channel to your firmware

```bash
ufbt update --channel=release   # official release firmware (default)
ufbt update --channel=dev       # bleeding-edge dev firmware
```

If your Flipper runs custom firmware (e.g. Momentum, RogueMaster) you may need to point uFBT at that firmware's SDK to avoid API version mismatches. See `ufbt update --help` for details.

### 3. Project layout

```
bible_viewer/
├── README.md
├── application.fam
├── bible_viewer.c
├── bible_data.h
├── icon.png
├── font/
│   ├── font.h
│   └── font.c
└── flipper_http/
    ├── flipper_http.h
    └── flipper_http.c
```

### 4. Build

Run `ufbt` from inside the `bible_viewer/` directory (the one containing `application.fam`):

```bash
cd bible_viewer
ufbt
```

The compiled `.fap` is placed in `dist/`. Copy it to your Flipper's SD card under `/ext/apps/Misc/`.

### 5. Build, install, and launch in one step

With your Flipper connected over USB (and qFlipper closed):

```bash
ufbt launch
```

This compiles, uploads, and starts the app on the device automatically.

### 6. SDK updates

```bash
ufbt update   # re-download SDK from previously selected channel
ufbt clean    # clear build cache if something goes wrong
```

---

## Architecture Notes

- **RAM usage:** ~18 KB offline, ~23 KB with WiFi active (FlipperHTTP freed when returning to main menu)
- **Verse data:** stored as plain text on SD card; only the current verse is loaded into RAM at a time; a lightweight index of file offsets is built on startup
- **Verse count table:** 1,189 `uint8_t` entries in flash — enables exact per-chapter verse clamping for all 66 books with no SD access
- **WiFi detection:** PING sent to the FlipperHTTP board on every API menu entry; `state` forced to `INACTIVE` after send so only a real `[PONG]` response can mark the board as present
- **RNG:** Xorshift32 seeded from `furi_get_tick()` — used for Random Verse and Verse of the Day selection
- **Verse of the Day:** chosen once per uptime-day; index and day counter persisted in `settings.txt`
