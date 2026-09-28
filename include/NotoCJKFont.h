#ifndef NOTO_CJK_FONT_H
#define NOTO_CJK_FONT_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define CJK_FONT_MAGIC_STR "CJKF"
#define CJK_MAX_BLOCKS 16
#define CJK_CACHE_SIZE 64

// Binary file header matching tools/generate_font.py
struct __attribute__((packed)) CJKFontHeader {
    char magic[4];            // "CJKF"
    uint16_t version;         // 1
    uint8_t glyph_width;      // 16
    uint8_t glyph_height;     // 16
    uint8_t bytes_per_glyph;  // 32
    uint8_t num_blocks;       // e.g. 10
    uint8_t reserved[6];
};

// Binary block descriptor matching tools/generate_font.py
struct __attribute__((packed)) CJKFontBlock {
    uint32_t start_cp;        // start Unicode codepoint (inclusive)
    uint32_t end_cp;          // end Unicode codepoint (inclusive)
    uint32_t file_offset;     // byte offset in font file
    uint8_t default_width;    // 8 for half-width, 16 for full-width
    uint8_t reserved[3];
};

// RAM cache entry for fast repeated character access
struct GlyphCacheEntry {
    uint32_t codepoint;
    uint8_t width;
    uint8_t bitmap[32];
    uint32_t last_used;
};

class NotoCJKFont {
public:
    NotoCJKFont();
    ~NotoCJKFont();

    // Mount font file from LittleFS
    bool begin(const char* fontPath = "/noto_cjk_16.bin");
    void end();

    bool isLoaded() const { return _loaded; }
    uint8_t getGlyphHeight() const { return _header.glyph_height; }

    // UTF-8 decoder: decodes one codepoint from utf8 string and advances ptr
    static uint32_t decodeUTF8(const char*& ptr);

    // Fetch glyph bitmap (cached or read from LittleFS)
    bool getGlyph(uint32_t cp, const uint8_t*& outBitmap, uint8_t& outWidth);

    // Draw single character by Unicode codepoint
    // Returns rendered width in pixels (or 0 if failed)
    int16_t drawChar(Adafruit_SSD1306& display, int16_t x, int16_t y, uint32_t cp,
                     uint16_t color = SSD1306_WHITE, uint16_t bg = SSD1306_BLACK);

    // Draw string at specified (x, y) coordinates without advancing global cursor
    // Returns total rendered width in pixels
    int16_t drawString(Adafruit_SSD1306& display, int16_t x, int16_t y, const char* str,
                       uint16_t color = SSD1306_WHITE, uint16_t bg = SSD1306_BLACK);

    // Terminal / Print style text rendering (auto-wrap, newline handling, auto-scroll)
    void print(Adafruit_SSD1306& display, const char* str);
    void println(Adafruit_SSD1306& display, const char* str = "");

    // Cursor and wrapping configuration
    void setCursor(int16_t x, int16_t y) { _cursorX = x; _cursorY = y; }
    int16_t getCursorX() const { return _cursorX; }
    int16_t getCursorY() const { return _cursorY; }
    void setTextWrap(bool wrap) { _textWrap = wrap; }
    void setTextColor(uint16_t color, uint16_t bg = SSD1306_BLACK) {
        _textColor = color;
        _textBg = bg;
    }

    // Measure string pixel width
    int16_t getTextWidth(const char* str);

private:
    File _fontFile;
    bool _loaded;
    CJKFontHeader _header;
    CJKFontBlock _blocks[CJK_MAX_BLOCKS];
    uint8_t _numBlocks;

    // LRU Cache
    GlyphCacheEntry _cache[CJK_CACHE_SIZE];
    uint32_t _accessCounter;

    // Terminal state
    int16_t _cursorX;
    int16_t _cursorY;
    uint16_t _textColor;
    uint16_t _textBg;
    bool _textWrap;

    // Direct pixel blit helper
    void blitGlyph(Adafruit_SSD1306& display, int16_t x, int16_t y,
                   const uint8_t* bitmap, uint8_t width, uint16_t color, uint16_t bg);
};

#endif // NOTO_CJK_FONT_H
