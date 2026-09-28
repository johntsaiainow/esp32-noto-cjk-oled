#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "NotoCJKFont.h"

// Hardware Configuration for ESP32-S3 DevKitC-1
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define I2C_SDA_PIN   8
#define I2C_SCL_PIN   9
#define I2C_SPEED_HZ  400000

#define FONT_FILE_PATH "/noto_cjk_16.bin"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
NotoCJKFont cjkFont;

// State management
enum Mode {
    MODE_DEMO,
    MODE_SERIAL_INPUT
};

Mode currentMode = MODE_DEMO;
uint8_t demoPage = 0;
const uint8_t TOTAL_PAGES = 6;
unsigned long lastPageSwitch = 0;
const unsigned long PAGE_DURATION_MS = 4000;
unsigned long lastSerialTime = 0;
const unsigned long SERIAL_TIMEOUT_MS = 15000;

String serialBuffer = "";

void renderPage(uint8_t page) {
    display.clearDisplay();

    switch (page) {
        case 0:
            // Intro / Title
            cjkFont.drawString(display, 0, 0,  "ESP32-S3 DevKit");
            cjkFont.drawString(display, 0, 16, "Noto Sans CJK");
            cjkFont.drawString(display, 0, 32, "16x16 Full Bitmap");
            cjkFont.drawString(display, 0, 48, "33,023 Glyphs!");
            break;

        case 1:
            // Simplified Chinese
            cjkFont.drawString(display, 0, 0,  "【简体中文】");
            cjkFont.drawString(display, 0, 16, "你好，世界！");
            cjkFont.drawString(display, 0, 32, "开源硬件 嵌入式");
            cjkFont.drawString(display, 0, 48, "物联天下 智慧未来");
            break;

        case 2:
            // Traditional Chinese
            cjkFont.drawString(display, 0, 0,  "【繁體中文】");
            cjkFont.drawString(display, 0, 16, "歡迎使用開源專案");
            cjkFont.drawString(display, 0, 32, "風調雨順 國泰民安");
            cjkFont.drawString(display, 0, 48, "數位時代 繁花似錦");
            break;

        case 3:
            // Japanese
            cjkFont.drawString(display, 0, 0,  "【日本語】");
            cjkFont.drawString(display, 0, 16, "こんにちは 世界！");
            cjkFont.drawString(display, 0, 32, "美しい日本の自然");
            cjkFont.drawString(display, 0, 48, "桜の花が咲きました");
            break;

        case 4:
            // Korean
            cjkFont.drawString(display, 0, 0,  "【한국어】");
            cjkFont.drawString(display, 0, 16, "안녕하세요, 세계!");
            cjkFont.drawString(display, 0, 32, "아름다운 한글 폰트");
            cjkFont.drawString(display, 0, 48, "임베디드 시스템");
            break;

        case 5: {
            // Mixed CJK & ASCII & Live Counter
            char lineBuf[32];
            snprintf(lineBuf, sizeof(lineBuf), "Uptime: %lusec", millis() / 1000);

            cjkFont.drawString(display, 0, 0,  "CJK: 中・日・韓");
            cjkFont.drawString(display, 0, 16, "Hello 123! @#$%");
            cjkFont.drawString(display, 0, 32, "Noto Sans Mono");
            cjkFont.drawString(display, 0, 48, lineBuf);
            break;
        }
    }

    display.display();
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n==========================================");
    Serial.println("  ESP32-S3 SSD1306 Noto CJK OLED Firmware ");
    Serial.println("==========================================");
    Serial.printf("I2C Pins: SDA=GPIO%d, SCL=GPIO%d @ 400kHz\n", I2C_SDA_PIN, I2C_SCL_PIN);

    // Initialize I2C bus
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_SPEED_HZ);

    // Try I2C addresses 0x3C, then 0x3D
    uint8_t oledAddr = 0x3C;
    if (!display.begin(SSD1306_SWITCHCAPVCC, oledAddr)) {
        oledAddr = 0x3D;
        if (!display.begin(SSD1306_SWITCHCAPVCC, oledAddr)) {
            Serial.println("[ERROR] SSD1306 OLED not found at 0x3C or 0x3D! Check wiring.");
            while (true) {
                delay(1000);
            }
        }
    }
    Serial.printf("[OK] SSD1306 OLED initialized at 0x%02X\n", oledAddr);

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SSD1306 OLED OK");
    display.println("Mounting LittleFS...");
    display.display();

    // Mount LittleFS and Noto CJK Font
    if (!cjkFont.begin(FONT_FILE_PATH)) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("FONT FILE MISSING!");
        display.println("");
        display.println("Upload LittleFS image:");
        display.println("pio run -t uploadfs");
        display.display();

        Serial.println("\n[ERROR] Font file '" FONT_FILE_PATH "' missing from LittleFS!");
        Serial.println("Please run: pio run -t uploadfs");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("[OK] Noto CJK Font ready!");
    Serial.println("\n--- Interactive Serial Input ---");
    Serial.println("Type any UTF-8 text (Chinese, Japanese, Korean, English)");
    Serial.println("and press Enter to display it directly on the OLED!");
    Serial.println("Commands:");
    Serial.println("  demo   - resume automatic demo carousel");
    Serial.println("  clear  - clear screen");
    Serial.println("--------------------------------\n");

    lastPageSwitch = millis();
    renderPage(0);
}

void loop() {
    // Check for incoming serial text
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r') continue;

        if (c == '\n') {
            serialBuffer.trim();
            if (serialBuffer.length() > 0) {
                if (serialBuffer.equalsIgnoreCase("demo")) {
                    currentMode = MODE_DEMO;
                    lastPageSwitch = millis();
                    renderPage(demoPage);
                    Serial.println("Resuming demo mode.");
                } else if (serialBuffer.equalsIgnoreCase("clear")) {
                    display.clearDisplay();
                    display.display();
                    cjkFont.setCursor(0, 0);
                    Serial.println("Screen cleared.");
                } else {
                    // Display user text on OLED
                    currentMode = MODE_SERIAL_INPUT;
                    lastSerialTime = millis();

                    display.clearDisplay();
                    cjkFont.setCursor(0, 0);
                    cjkFont.println(display, serialBuffer.c_str());
                    display.display();

                    Serial.printf("Displayed on OLED: %s\n", serialBuffer.c_str());
                }
                serialBuffer = "";
            }
        } else {
            serialBuffer += c;
        }
    }

    // Mode handling
    if (currentMode == MODE_DEMO) {
        if (millis() - lastPageSwitch >= PAGE_DURATION_MS) {
            lastPageSwitch = millis();
            demoPage = (demoPage + 1) % TOTAL_PAGES;
            renderPage(demoPage);
        }
    } else if (currentMode == MODE_SERIAL_INPUT) {
        // Return to demo mode after inactivity timeout
        if (millis() - lastSerialTime >= SERIAL_TIMEOUT_MS) {
            currentMode = MODE_DEMO;
            lastPageSwitch = millis();
            renderPage(demoPage);
            Serial.println("Serial idle timeout: resuming demo mode.");
        }
    }

    delay(20);
}