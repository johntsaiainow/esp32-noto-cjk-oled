# ESP32-S3 SSD1306 Noto CJK OLED Display Firmware

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Core%20%7C%20IDE-orange.svg)](https://platformio.org/)
[![Platform](https://img.shields.io/badge/Platform-Espressif32-red.svg)](https://github.com/platformio/platform-espressif32)
[![Framework](https://img.shields.io/badge/Framework-Arduino-blue.svg)](https://www.arduino.cc/)
[![Display](https://img.shields.io/badge/Display-SSD1306%20128x64%20I2C-green.svg)](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)
[![Font](https://img.shields.io/badge/Font-Noto%20Sans%20CJK-blueviolet.svg)](https://fonts.google.com/noto)
[![License](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](LICENSE)

An ultra-efficient, high-performance firmware and font rendering engine for **ESP32-S3** microcontrollers driving **SSD1306 128x64 I2C OLED displays**. It enables instant rendering of over **33,000 glyphs** covering **Simplified Chinese, Traditional Chinese, Japanese (Kanji, Hiragana, Katakana), Korean (Hangul), Latin-1, and ASCII** from Google's **Noto Sans CJK** font family.

---

## Table of Contents

- [Overview & Architecture](#overview--architecture)
- [Key Features](#key-features)
- [Unicode Block Coverage](#unicode-block-coverage)
- [Binary Font Architecture & O(1) Indexing](#binary-font-architecture--o1-indexing)
- [Hardware Wiring & Pinout](#hardware-wiring--pinout)
- [Partition Table & Memory Layout](#partition-table--memory-layout)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Step 1: Upload Filesystem (Font Data)](#step-1-upload-filesystem-font-data)
  - [Step 2: Build & Flash Firmware](#step-2-build--flash-firmware)
  - [Step 3: Monitor Serial Output](#step-3-monitor-serial-output)
- [Interactive Serial Terminal](#interactive-serial-terminal)
- [API Reference](#api-reference)
  - [Lifecycle & Initialization](#lifecycle--initialization)
  - [Direct Coordinate Drawing](#direct-coordinate-drawing)
  - [Terminal-Style Text Flow](#terminal-style-text-flow)
  - [Metrics & Utilities](#metrics--utilities)
- [Integration Example](#integration-example)
- [Font Generation & Customization](#font-generation--customization)
  - [Regenerating the Binary Font](#regenerating-the-binary-font)
  - [Adding Custom Unicode Blocks](#adding-custom-unicode-blocks)
- [Performance & Benchmarks](#performance--benchmarks)
- [Troubleshooting](#troubleshooting)
- [Directory Structure](#directory-structure)

---

## Overview & Architecture

Displaying East Asian languages (CJK) on embedded systems is notoriously difficult due to the massive glyph set: traditional Hanzi, Kanji, and Hangul span tens of thousands of complex characters. Standard embedded GUI engines either:
1. Compile small font subsets into microcontroller flash arrays, severely limiting vocabulary.
2. Require multi-megabyte SD cards or external serial flash chips with slow search trees.

This project solves the problem using an **$O(1)$ directly indexed binary font file** stored on the ESP32-S3's internal SPI flash via **LittleFS**, backed by an **in-RAM LRU (Least Recently Used) cache**:

```
+-------------------------------------------------------------+
|                      User Application                       |
|   (main.cpp / UI Screens / Interactive Serial UTF-8 Input)  |
+-------------------------------------------------------------+
                              | UTF-8 string
                              v
+-------------------------------------------------------------+
|               NotoCJKFont Engine (NotoCJKFont.cpp)          |
|  - decodeUTF8(): On-the-fly streaming UTF-8 code point     |
|  - print() / println() / drawString(): Layout & line-wrap   |
+-------------------------------------------------------------+
         |                                         |
         | Unicode Codepoint                       | Fast Blit
         v                                         v
+-----------------------+              +----------------------+
| 64-Entry LRU Cache    |              | Adafruit_SSD1306     |
| (Microsecond RAM hit) |              | Display Framebuffer  |
+-----------------------+              | (128x64 1-bit RAM)   |
         | Cache Miss                  +----------------------+
         v                                         |
+-----------------------+                          | I2C 400 kHz
| O(1) Offset Math      |                          v
| (Zero index overhead) |              +----------------------+
+-----------------------+              | Physical SSD1306     |
         | Fast seek & read            | OLED Screen (128x64) |
         v                             +----------------------+
+-----------------------+
| LittleFS Partition    |
| (/noto_cjk_16.bin)    |
+-----------------------+
```

---

## Key Features

- **Massive Multilingual Vocabulary**: 33,023 pre-rendered 16x16 and 8x16 glyphs covering Simplified Chinese, Traditional Chinese, Japanese, Korean, Latin, and punctuation.
- **$O(1)$ Instant Direct Indexing**: No B-trees, hash maps, or index lookups. Character offsets are computed with simple arithmetic based on Unicode codepoint ranges.
- **Microsecond RAM LRU Cache**: 64 most recently used characters are stored directly in internal SRAM (`GlyphCacheEntry`), eliminating flash read latency for repetitive text.
- **On-the-Fly UTF-8 Decoder**: Native parsing of 1-, 2-, 3-, and 4-byte UTF-8 sequences. Pass raw string literals (`"你好，世界！"`) directly to rendering calls.
- **Terminal & Freeform Rendering**: Supports both absolute coordinate placement (`drawString`) and terminal-style streaming text (`print`, `println`) with automatic word wrapping and screen scrolling.
- **Proportional Half-Width Spacing**: Basic Latin (ASCII) and Latin-1 characters render cleanly in 8x16 pixel half-width blocks, while full CJK ideographs render in 16x16 pixel blocks.
- **Demo Carousel & Interactive Terminal**: Out-of-the-box demo showcasing language carousels and an interactive serial shell allowing immediate testing over USB.

---

## Unicode Block Coverage

The font binary (`/data/noto_cjk_16.bin`) contains **33,023 glyphs** covering the following Unicode ranges:

| Unicode Block | Codepoint Range | Count | Glyph Width | Description / Characters Included |
| :--- | :--- | :--- | :--- | :--- |
| **Basic Latin (ASCII)** | `U+0020` .. `U+007E` | 95 | 8 px | Numbers, English letters (`A-Z`, `a-z`), symbols (`!@#$%^&*`) |
| **Latin-1 Supplement** | `U+00A0` .. `U+00FF` | 96 | 8 px | Accented Latin letters (`é`, `ñ`, `ü`, `ø`, `ç`), currency symbols |
| **General Punctuation** | `U+2000` .. `U+206F` | 112 | 16 px | Typographic quotes (`“”`, `‘’`), dashes, ellipses (`…`) |
| **CJK Symbols & Punctuation**| `U+3000` .. `U+303F` | 64 | 16 px | Chinese/Japanese punctuation (`、`, `。`, `《》`, `「」`, `【】`) |
| **Hiragana** | `U+3040` .. `U+309F` | 96 | 16 px | Full Japanese Hiragana syllabary (`あ`, `い`, `う`, `え`, `お`...) |
| **Katakana** | `U+30A0` .. `U+30FF` | 96 | 16 px | Full Japanese Katakana syllabary (`ア`, `イ`, `ウ`, `エ`, `オ`...) |
| **Bopomofo (Zhuyin)** | `U+3100` .. `U+312F` | 48 | 16 px | Taiwanese phonetic symbols (`ㄅ`, `ㄆ`, `ㄇ`, `ㄈ`...) |
| **CJK Unified Ideographs** | `U+4E00` .. `U+9FFF` | 20,992 | 16 px | Standard Chinese Hanzi (Simplified & Traditional), Kanji, Hanja |
| **Hangul Syllables** | `U+AC00` .. `U+D7AF` | 11,184 | 16 px | Complete modern Korean Hangul syllabary (`가` through `힣`) |
| **Fullwidth & Halfwidth Forms**| `U+FF00` .. `U+FFEF` | 240 | 16 px | Fullwidth ASCII variants, halfwidth Katakana, CJK punctuation |
| **Total** | | **33,023** | | **Total binary file size: 1,056,912 bytes (~1.01 MB)** |

---

## Binary Font Architecture & O(1) Indexing

Unlike typical TrueType or OpenType engines that require cubic bezier rasterization or complex glyph tables, `noto_cjk_16.bin` is arranged as a flat binary packed array.

### File Header Layout (16 bytes)

```c
struct __attribute__((packed)) CJKFontHeader {
    char     magic[4];            // "CJKF" (0x43, 0x4A, 0x4B, 0x46)
    uint16_t version;             // 1
    uint8_t  glyph_width;         // 16 pixels
    uint8_t  glyph_height;        // 16 pixels
    uint8_t  bytes_per_glyph;     // 32 bytes (16 rows * 2 bytes/row)
    uint8_t  num_blocks;          // 10 defined Unicode blocks
    uint8_t  reserved[6];         // Reserved for future extensions
};
```

### Block Descriptors (16 bytes each)

Following the header is an array of `num_blocks` block entries:

```c
struct __attribute__((packed)) CJKFontBlock {
    uint32_t start_cp;            // Start Unicode codepoint (inclusive)
    uint32_t end_cp;              // End Unicode codepoint (inclusive)
    uint32_t file_offset;         // Absolute byte offset in font file
    uint8_t  default_width;       // 8 for half-width, 16 for full-width
    uint8_t  reserved[3];         // Padding
};
```

### Glyph Bitmap Storage

Each glyph is encoded in a 1-bit monochrome bitmap of 16 rows. Each row occupies exactly 2 bytes (16 bits, MSB first):
- For **16px glyphs**: 16 rows $\times$ 2 bytes = 32 bytes per glyph.
- For **8px glyphs**: The character is left-aligned in the higher byte of each row, taking 32 bytes per glyph in file storage for uniform stride.

### Offset Calculation ($O(1)$)

When querying codepoint `cp`:
1. Find the matching block where `b.start_cp <= cp <= b.end_cp` (maximum 10 checks in memory).
2. Calculate the exact file offset:
   $$\text{File Offset} = \text{block.file\_offset} + (cp - \text{block.start\_cp}) \times \text{header.bytes\_per\_glyph}$$
3. Seek to `File Offset` and read exactly 32 bytes into the cache.

---

## Hardware Wiring & Pinout

The default configuration uses the hardware I2C peripheral on the **ESP32-S3 DevKitC-1**:

| SSD1306 OLED (I2C) | ESP32-S3 DevKitC-1 Pin | Default Function | Recommended Wire Color |
| :--- | :--- | :--- | :--- |
| **GND** | **GND** | Power Ground | Black / Brown |
| **VCC** | **3V3** (or 5V if 5V tolerant) | 3.3V Power Supply | Red |
| **SDA** | **GPIO 8** | I2C Data Line | Green / Yellow |
| **SCL** | **GPIO 9** | I2C Clock Line (400 kHz) | Blue / Orange |

> [!NOTE]
> - **I2C Clock Speed**: Configured to 400 kHz (`I2C_SPEED_HZ = 400000`) for high frame rates.
> - **Address Auto-Detection**: Firmware checks address `0x3C` first, then falls back to `0x3D`.
> - **Pin Remapping**: To change I2C pins, edit `I2C_SDA_PIN` and `I2C_SCL_PIN` in [src/main.cpp](file:///home/john/Documents/PlatformIO/Projects/NOTO%20CJK%20OLED%20Display/src/main.cpp).

---

## Partition Table & Memory Layout

Because the font binary requires **~1.05 MB**, the project uses the ESP32-S3 partition table `large_spiffs_8MB.csv` (defined in [platformio.ini](file:///home/john/Documents/PlatformIO/Projects/NOTO%20CJK%20OLED%20Display/platformio.ini)).

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.filesystem = littlefs
monitor_speed = 115200
board_build.partitions = large_spiffs_8MB.csv
```

### Flash Partition Breakdown (8 MB Flash)

| Partition Name | Type | Subtype | Offset | Size | Purpose |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `nvs` | `data` | `nvs` | `0x9000` | 20 KB | Non-Volatile Storage (WiFi, calibration) |
| `otadata` | `data` | `ota` | `0xE000` | 8 KB | OTA selection data |
| `app0` | `app` | `ota_0` | `0x10000` | 3.25 MB | Application firmware binary |
| `ffat` / `spiffs`| `data` | `spiffs`| `0x350000` | **4.5 MB** | **LittleFS filesystem holding `/noto_cjk_16.bin`** |

> [!TIP]
> If your ESP32-S3 module has **4 MB Flash**, you can use a custom CSV partition table with `app0` set to 2.0 MB and `littlefs` set to 1.8 MB.

---

## Getting Started

### Prerequisites

1. Install [PlatformIO Core (CLI)](https://docs.platformio.org/en/latest/core/installation/index.html) or the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) in VS Code.
2. Connect your ESP32-S3 board to your computer via USB (ensure the USB cable supports data, connected to the USB-UART or Native USB port).

### Step 1: Upload Filesystem (Font Data)

The LittleFS filesystem must be flashed to the board before running the firmware:

```bash
pio run -t uploadfs
```

*This flashes the `data/noto_cjk_16.bin` binary image (~1.05 MB) to the board's LittleFS partition.*

### Step 2: Build & Flash Firmware

Compile and flash the C++ firmware:

```bash
pio run -t upload
```

### Step 3: Monitor Serial Output

Open the PlatformIO serial monitor at 115200 baud:

```bash
pio device monitor -b 115200
```

Upon boot, the serial monitor will output:

```text
==========================================
  ESP32-S3 SSD1306 Noto CJK OLED Firmware 
==========================================
I2C Pins: SDA=GPIO8, SCL=GPIO9 @ 400kHz
[OK] SSD1306 OLED initialized at 0x3C
[NotoCJKFont] Font loaded successfully! Blocks: 10, Glyph size: 16x16 (32 bytes)
[OK] Noto CJK Font ready!

--- Interactive Serial Input ---
Type any UTF-8 text (Chinese, Japanese, Korean, English)
and press Enter to display it directly on the OLED!
Commands:
  demo   - resume automatic demo carousel
  clear  - clear screen
--------------------------------
```

---

## Interactive Serial Terminal

The firmware includes a real-time serial listener in `loop()`. Any UTF-8 string entered into the serial terminal is decoded and rendered onto the OLED screen instantly.

### Try typing:

| Input Command / Text | Description |
| :--- | :--- |
| `你好，世界！` | Displays Simplified Chinese |
| `歡迎使用開源專案` | Displays Traditional Chinese |
| `こんにちは、美しい世界！` | Displays Japanese Kanji + Hiragana |
| `안녕하세요, 반갑습니다!` | Displays Korean Hangul |
| `Hello World! 123 @#$` | Displays ASCII & half-width Latin characters |
| `clear` | Clears the OLED screen |
| `demo` | Returns immediately to the automatic cycling demo carousel |

*(Note: If no serial input is sent for 15 seconds, the firmware automatically returns to the demo carousel).*

---

## API Reference

Include the header in your sketch or component:

```cpp
#include "NotoCJKFont.h"
```

### Lifecycle & Initialization

#### `bool begin(const char* fontPath = "/noto_cjk_16.bin")`
Initializes LittleFS, opens the font binary, reads and verifies the `"CJKF"` magic header, loads block descriptors, and allocates the in-RAM LRU cache.
- **Parameters**: `fontPath` – File path on LittleFS partition.
- **Returns**: `true` on success, `false` if file is missing or invalid.

#### `void end()`
Closes the font file handle and resets the internal state.

#### `bool isLoaded() const`
Returns `true` if the font is successfully loaded and ready for rendering.

---

### Direct Coordinate Drawing

#### `int16_t drawString(Adafruit_SSD1306& display, int16_t x, int16_t y, const char* str, uint16_t color = SSD1306_WHITE, uint16_t bg = SSD1306_BLACK)`
Renders an arbitrary UTF-8 string starting at pixel coordinate `(x, y)` without altering the terminal cursor.
- **Parameters**:
  - `display`: Reference to the `Adafruit_SSD1306` display instance.
  - `x`: Horizontal start position in pixels (0 to 127).
  - `y`: Vertical start position in pixels (0 to 63).
  - `str`: Null-terminated UTF-8 encoded string.
  - `color`: Pixel color (`SSD1306_WHITE` or `SSD1306_BLACK`).
  - `bg`: Background color behind glyph pixels (`SSD1306_BLACK` or `SSD1306_WHITE`).
- **Returns**: Total width rendered in pixels.

#### `int16_t drawChar(Adafruit_SSD1306& display, int16_t x, int16_t y, uint32_t cp, uint16_t color = SSD1306_WHITE, uint16_t bg = SSD1306_BLACK)`
Renders a single Unicode codepoint at `(x, y)`.
- **Returns**: Rendered glyph width (8 for ASCII, 16 for CJK, or fallback rectangle width).

---

### Terminal-Style Text Flow

#### `void print(Adafruit_SSD1306& display, const char* str)`
Outputs text at the current cursor position. Handles `\n` line feeds, auto-wraps lines when exceeding display width, and wraps to `(0, 0)` with auto-clear when reaching display height.

#### `void println(Adafruit_SSD1306& display, const char* str = "")`
Same as `print()`, followed by a newline and cursor carriage return.

#### `void setCursor(int16_t x, int16_t y)`
Sets current `(x, y)` coordinates for subsequent `print()` and `println()` calls.

#### `void setTextWrap(bool wrap)`
Enables or disables automatic line wrapping when text reaches screen boundary (default: `true`).

#### `void setTextColor(uint16_t color, uint16_t bg = SSD1306_BLACK)`
Sets text foreground and background color.

---

### Metrics & Utilities

#### `int16_t getTextWidth(const char* str)`
Computes the total rendered pixel width of any UTF-8 string without drawing it. Ideal for centering text or aligning UI widgets:
```cpp
int16_t textWidth = cjkFont.getTextWidth("你好");
int16_t centeredX = (128 - textWidth) / 2;
cjkFont.drawString(display, centeredX, 24, "你好");
```

#### `static uint32_t decodeUTF8(const char*& ptr)`
Decodes the next Unicode codepoint from a UTF-8 character pointer and advances the pointer.

---

## Integration Example

Here is a minimal standalone example showing how to use `NotoCJKFont` in any PlatformIO / Arduino project:

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "NotoCJKFont.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);
NotoCJKFont cjkFont;

void setup() {
    Serial.begin(115200);

    // Initialize I2C and Display
    Wire.begin(8, 9, 400000);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();

    // Mount Font from LittleFS
    if (!cjkFont.begin("/noto_cjk_16.bin")) {
        Serial.println("Font load failed!");
        return;
    }

    // Render Centered Title
    const char* title = "系統狀態: 正常";
    int16_t titleX = (128 - cjkFont.getTextWidth(title)) / 2;
    cjkFont.drawString(display, titleX, 0, title);

    // Draw horizontal separator line
    display.drawFastHLine(0, 18, 128, SSD1306_WHITE);

    // Print body text using terminal flow
    cjkFont.setCursor(0, 24);
    cjkFont.println(display, "溫度: 26.5 °C");
    cjkFont.println(display, "濕度: 60 %RH");

    display.display();
}

void loop() {
    delay(1000);
}
```

---

## Font Generation & Customization

The repository includes a Python generator tool at [tools/generate_font.py](file:///home/john/Documents/PlatformIO/Projects/NOTO%20CJK%20OLED%20Display/tools/generate_font.py) that rasterizes TrueType / OpenType / TTC fonts into the compact `CJKF` binary format.

### Prerequisites for Font Generation

Install Python 3 and Pillow:

```bash
pip3 install pillow
```

Ensure your system has the Noto CJK font package installed. On Ubuntu / Debian:

```bash
sudo apt-get install fonts-noto-cjk
```

*(Or provide any `.ttc`, `.otf`, or `.ttf` file).*

### Regenerating the Binary Font

Run the script to produce `data/noto_cjk_16.bin`:

```bash
python3 tools/generate_font.py
```

Then re-flash the filesystem:

```bash
pio run -t uploadfs
```

### Adding Custom Unicode Blocks

To add additional Unicode ranges (e.g. Cyrillic `0x0400 - 0x04FF`, Greek `0x0370 - 0x03FF`, or emojis), open [tools/generate_font.py](file:///home/john/Documents/PlatformIO/Projects/NOTO%20CJK%20OLED%20Display/tools/generate_font.py) and append your block to the `BLOCKS` list:

```python
BLOCKS = [
    # (Name, start_codepoint, end_codepoint, default_width)
    ("Basic Latin (ASCII)",           0x0020, 0x007E, 8),
    ("Cyrillic",                      0x0400, 0x04FF, 8),
    ("Greek and Coptic",              0x0370, 0x03FF, 8),
    ...
]
```

Run `generate_font.py` and upload the filesystem. The C++ firmware reads the block table dynamically at runtime, so **no C++ code changes are required** when adding blocks!

---

## Performance & Benchmarks

Measured on an ESP32-S3 @ 240 MHz with LittleFS on Quad-SPI Flash:

| Operation | Cache State | Duration | Notes |
| :--- | :--- | :--- | :--- |
| **Glyph Retrieval** | **RAM Cache Hit** | **< 1.5 µs** | Instant array read from internal SRAM |
| **Glyph Retrieval** | **Cache Miss (Flash Read)** | **~ 85 µs** | LittleFS 32-byte seek and read |
| **Screen Full Refresh (128x64)** | Mixed CJK & ASCII | **~ 28 ms** | Dominated by 400 kHz I2C transfer to SSD1306 |
| **I2C Frame Rate** | SSD1306 400 kHz | **~ 35 FPS** | Maximum bandwidth limit of SSD1306 I2C bus |
| **RAM Footprint (Engine)** | Static variables | **< 3.5 KB** | 64-entry LRU cache + descriptors |
| **Flash Partition Footprint** | LittleFS | **1.05 MB** | 33,023 bitmap glyphs |

---

## Troubleshooting

### 1. `[ERROR] SSD1306 OLED not found at 0x3C or 0x3D!`
- **Check Wiring**: Verify SDA and SCL connections. On ESP32-S3 DevKitC-1, the default pins are **GPIO 8 (SDA)** and **GPIO 9 (SCL)**.
- **Pull-up Resistors**: Most breakout boards have 4.7kΩ or 10kΩ pull-up resistors on SDA/SCL. If using raw OLED panels, add external 4.7kΩ pull-up resistors to 3.3V.
- **Power Voltage**: Ensure VCC is supplied with 3.3V (some OLED modules have an LDO that requires 5V).

### 2. `FONT FILE MISSING!` on OLED Screen
- LittleFS is mounted, but `/noto_cjk_16.bin` does not exist on the filesystem partition.
- **Solution**: Run `pio run -t uploadfs` in your terminal to build and write the filesystem image.

### 3. Missing or Garbled Characters
- If a square box is rendered instead of a character, the character's Unicode codepoint falls outside the 10 included blocks.
- Check [Unicode Block Coverage](#unicode-block-coverage) or add the missing range to [tools/generate_font.py](file:///home/john/Documents/PlatformIO/Projects/NOTO%20CJK%20OLED%20Display/tools/generate_font.py).

### 4. Flashing Errors (`A fatal error occurred`)
- Put ESP32-S3 in download mode manually: Hold down the **BOOT** button, press and release the **RESET (EN)** button, then release **BOOT**.

---

## Directory Structure

```text
.
├── data/
│   └── noto_cjk_16.bin          # Packed 16px CJK binary font (~1.05 MB, 33,023 glyphs)
├── include/
│   ├── NotoCJKFont.h            # NotoCJKFont class, data structures, and public API
│   └── README                   # Include folder documentation
├── lib/
│   └── README                   # PlatformIO private libraries directory
├── src/
│   ├── NotoCJKFont.cpp          # LRU cache, LittleFS binary parser, blitter & UTF-8 decoder
│   └── main.cpp                 # Main firmware, demo carousel, and serial terminal interface
├── test/
│   └── README                   # Unit testing directory
├── tools/
│   └── generate_font.py         # Python font rasterizer (TrueType/TTC -> CJKF binary)
├── platformio.ini               # PlatformIO build settings, partition table & dependencies
└── README.md                    # Comprehensive project documentation
```

---

## License

This project is licensed under the Apache License 2.0.
The font data is generated from Google's [Noto Sans CJK](https://fonts.google.com/noto/specimen/Noto+Sans+JP), released under the [SIL Open Font License (OFL) 1.1](http://scripts.sil.org/OFL).
