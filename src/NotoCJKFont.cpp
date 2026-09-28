#include "NotoCJKFont.h"

NotoCJKFont::NotoCJKFont()
    : _loaded(false), _numBlocks(0), _accessCounter(0),
      _cursorX(0), _cursorY(0), _textColor(SSD1306_WHITE), _textBg(SSD1306_BLACK), _textWrap(true) {
    memset(&_header, 0, sizeof(_header));
    memset(_blocks, 0, sizeof(_blocks));
    memset(_cache, 0, sizeof(_cache));
}

NotoCJKFont::~NotoCJKFont() {
    end();
}

bool NotoCJKFont::begin(const char* fontPath) {
    end();

    if (!LittleFS.begin(false)) {
        Serial.println("[NotoCJKFont] LittleFS mount failed!");
        return false;
    }

    if (!LittleFS.exists(fontPath)) {
        Serial.printf("[NotoCJKFont] Font file '%s' not found on LittleFS!\n", fontPath);
        return false;
    }

    _fontFile = LittleFS.open(fontPath, "r");
    if (!_fontFile) {
        Serial.printf("[NotoCJKFont] Failed to open '%s'\n", fontPath);
        return false;
    }

    // Read header
    if (_fontFile.read((uint8_t*)&_header, sizeof(_header)) != sizeof(_header)) {
        Serial.println("[NotoCJKFont] Header read error");
        _fontFile.close();
        return false;
    }

    if (memcmp(_header.magic, CJK_FONT_MAGIC_STR, 4) != 0) {
        Serial.printf("[NotoCJKFont] Invalid magic: %c%c%c%c\n",
                      _header.magic[0], _header.magic[1], _header.magic[2], _header.magic[3]);
        _fontFile.close();
        return false;
    }

    _numBlocks = _header.num_blocks;
    if (_numBlocks > CJK_MAX_BLOCKS) {
        _numBlocks = CJK_MAX_BLOCKS;
    }

    // Read block descriptors
    size_t blocksSize = _numBlocks * sizeof(CJKFontBlock);
    if (_fontFile.read((uint8_t*)_blocks, blocksSize) != blocksSize) {
        Serial.println("[NotoCJKFont] Block table read error");
        _fontFile.close();
        return false;
    }

    // Clear LRU cache
    for (int i = 0; i < CJK_CACHE_SIZE; i++) {
        _cache[i].codepoint = 0xFFFFFFFF;
        _cache[i].last_used = 0;
    }
    _accessCounter = 0;
    _cursorX = 0;
    _cursorY = 0;
    _loaded = true;

    Serial.printf("[NotoCJKFont] Font loaded successfully! Blocks: %d, Glyph size: %dx%d (%d bytes)\n",
                  _numBlocks, _header.glyph_width, _header.glyph_height, _header.bytes_per_glyph);
    return true;
}

void NotoCJKFont::end() {
    if (_loaded && _fontFile) {
        _fontFile.close();
    }
    _loaded = false;
}

uint32_t NotoCJKFont::decodeUTF8(const char*& ptr) {
    if (!ptr || *ptr == '\0') return 0;

    uint8_t c = (uint8_t)*ptr++;
    if (c < 0x80) {
        return c;
    } else if ((c & 0xE0) == 0xC0) {
        if (!*ptr) return 0;
        uint32_t cp = (c & 0x1F) << 6;
        cp |= ((uint8_t)*ptr++) & 0x3F;
        return cp;
    } else if ((c & 0xF0) == 0xE0) {
        if (!*ptr) return 0;
        uint32_t cp = (c & 0x0F) << 12;
        cp |= (((uint8_t)*ptr++) & 0x3F) << 6;
        if (!*ptr) return 0;
        cp |= ((uint8_t)*ptr++) & 0x3F;
        return cp;
    } else if ((c & 0xF8) == 0xF0) {
        if (!*ptr) return 0;
        uint32_t cp = (c & 0x07) << 18;
        cp |= (((uint8_t)*ptr++) & 0x3F) << 12;
        if (!*ptr) return 0;
        cp |= (((uint8_t)*ptr++) & 0x3F) << 6;
        if (!*ptr) return 0;
        cp |= ((uint8_t)*ptr++) & 0x3F;
        return cp;
    }
    return 0; // Malformed byte
}

bool NotoCJKFont::getGlyph(uint32_t cp, const uint8_t*& outBitmap, uint8_t& outWidth) {
    if (!_loaded) return false;

    // Check RAM cache
    for (int i = 0; i < CJK_CACHE_SIZE; i++) {
        if (_cache[i].codepoint == cp) {
            _cache[i].last_used = ++_accessCounter;
            outBitmap = _cache[i].bitmap;
            outWidth = _cache[i].width;
            return true;
        }
    }

    // Locate matching block
    const CJKFontBlock* targetBlock = nullptr;
    for (int i = 0; i < _numBlocks; i++) {
        if (cp >= _blocks[i].start_cp && cp <= _blocks[i].end_cp) {
            targetBlock = &_blocks[i];
            break;
        }
    }

    if (!targetBlock) {
        return false;
    }

    // Find LRU slot in cache
    int lruIdx = 0;
    uint32_t minAge = 0xFFFFFFFF;
    for (int i = 0; i < CJK_CACHE_SIZE; i++) {
        if (_cache[i].last_used < minAge) {
            minAge = _cache[i].last_used;
            lruIdx = i;
        }
    }

    // Calculate file offset
    uint32_t charIdx = cp - targetBlock->start_cp;
    uint32_t fileOffset = targetBlock->file_offset + charIdx * _header.bytes_per_glyph;

    _fontFile.seek(fileOffset);
    if (_fontFile.read(_cache[lruIdx].bitmap, _header.bytes_per_glyph) != _header.bytes_per_glyph) {
        return false;
    }

    _cache[lruIdx].codepoint = cp;
    _cache[lruIdx].width = targetBlock->default_width;
    _cache[lruIdx].last_used = ++_accessCounter;

    outBitmap = _cache[lruIdx].bitmap;
    outWidth = _cache[lruIdx].width;
    return true;
}

void NotoCJKFont::blitGlyph(Adafruit_SSD1306& display, int16_t x, int16_t y,
                            const uint8_t* bitmap, uint8_t width, uint16_t color, uint16_t bg) {
    int16_t dispW = display.width();
    int16_t dispH = display.height();

    for (int r = 0; r < 16; r++) {
        int16_t py = y + r;
        if (py < 0 || py >= dispH) continue;

        uint16_t rowBits = (bitmap[r * 2] << 8) | bitmap[r * 2 + 1];
        for (int c = 0; c < width; c++) {
            int16_t px = x + c;
            if (px < 0 || px >= dispW) continue;

            bool pixelOn = (rowBits & (0x8000 >> c)) != 0;
            if (pixelOn) {
                display.drawPixel(px, py, color);
            } else if (bg != color) {
                display.drawPixel(px, py, bg);
            }
        }
    }
}

int16_t NotoCJKFont::drawChar(Adafruit_SSD1306& display, int16_t x, int16_t y, uint32_t cp,
                              uint16_t color, uint16_t bg) {
    if (cp == ' ') {
        uint8_t spaceWidth = 8;
        if (bg != color) {
            display.fillRect(x, y, spaceWidth, _header.glyph_height, bg);
        }
        return spaceWidth;
    }

    const uint8_t* bitmap = nullptr;
    uint8_t width = 0;

    if (getGlyph(cp, bitmap, width)) {
        blitGlyph(display, x, y, bitmap, width, color, bg);
        return width;
    } else {
        // Fallback: draw an 8x16 box
        uint8_t fallbackWidth = (cp > 0x2000) ? 16 : 8;
        display.drawRect(x, y, fallbackWidth, _header.glyph_height, color);
        return fallbackWidth;
    }
}

int16_t NotoCJKFont::drawString(Adafruit_SSD1306& display, int16_t x, int16_t y, const char* str,
                                uint16_t color, uint16_t bg) {
    const char* ptr = str;
    int16_t curX = x;

    while (*ptr) {
        uint32_t cp = decodeUTF8(ptr);
        if (cp == 0) continue;
        int16_t w = drawChar(display, curX, y, cp, color, bg);
        curX += w;
    }

    return curX - x;
}

void NotoCJKFont::print(Adafruit_SSD1306& display, const char* str) {
    const char* ptr = str;
    int16_t dispW = display.width();
    int16_t dispH = display.height();
    int16_t lineH = _header.glyph_height ? _header.glyph_height : 16;

    while (*ptr) {
        if (*ptr == '\r') {
            ptr++;
            continue;
        }

        if (*ptr == '\n') {
            ptr++;
            _cursorX = 0;
            _cursorY += lineH;
            if (_cursorY + lineH > dispH) {
                _cursorY = 0;
                display.clearDisplay();
            }
            continue;
        }

        uint32_t cp = decodeUTF8(ptr);
        if (cp == 0) continue;

        // Estimate character width
        uint8_t charWidth = 8;
        const uint8_t* dummyBmp = nullptr;
        if (!getGlyph(cp, dummyBmp, charWidth)) {
            charWidth = (cp > 0x2000) ? 16 : 8;
        }

        // Handle word wrap
        if (_textWrap && (_cursorX + charWidth > dispW)) {
            _cursorX = 0;
            _cursorY += lineH;
            if (_cursorY + lineH > dispH) {
                _cursorY = 0;
                display.clearDisplay();
            }
        }

        int16_t w = drawChar(display, _cursorX, _cursorY, cp, _textColor, _textBg);
        _cursorX += w;
    }
}

void NotoCJKFont::println(Adafruit_SSD1306& display, const char* str) {
    print(display, str);
    int16_t lineH = _header.glyph_height ? _header.glyph_height : 16;
    _cursorX = 0;
    _cursorY += lineH;
    if (_cursorY + lineH > display.height()) {
        _cursorY = 0;
        display.clearDisplay();
    }
}

int16_t NotoCJKFont::getTextWidth(const char* str) {
    const char* ptr = str;
    int16_t total = 0;

    while (*ptr) {
        uint32_t cp = decodeUTF8(ptr);
        if (cp == 0) continue;
        if (cp == ' ') {
            total += 8;
            continue;
        }
        const uint8_t* bmp = nullptr;
        uint8_t w = 8;
        if (getGlyph(cp, bmp, w)) {
            total += w;
        } else {
            total += (cp > 0x2000) ? 16 : 8;
        }
    }
    return total;
}
