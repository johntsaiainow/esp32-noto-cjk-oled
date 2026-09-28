#!/usr/bin/env python3
"""
Generate a binary CJK bitmap font from Noto Sans CJK for ESP32 LittleFS.
The binary font is structured for instant O(1) random access by Unicode codepoint.
"""

import os
import sys
import time
import struct
from PIL import Image, ImageFont, ImageDraw

FONT_CANDIDATES = [
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
]

# Unicode blocks to include
BLOCKS = [
    # (name, start_cp, end_cp, default_width)
    ("Basic Latin (ASCII)",               0x0020, 0x007E, 8),
    ("Latin-1 Supplement",                0x00A0, 0x00FF, 8),
    ("General Punctuation & Symbols",     0x2000, 0x206F, 16),
    ("CJK Symbols & Punctuation",         0x3000, 0x303F, 16),
    ("Hiragana",                          0x3040, 0x309F, 16),
    ("Katakana",                          0x30A0, 0x30FF, 16),
    ("Bopomofo",                          0x3100, 0x312F, 16),
    ("CJK Unified Ideographs",            0x4E00, 0x9FFF, 16),
    ("Hangul Syllables",                  0xAC00, 0xD7AF, 16),
    ("Fullwidth & Halfwidth Forms",       0xFF00, 0xFFEF, 16),
]

MAGIC = b"CJKF"
VERSION = 1
GLYPH_WIDTH = 16
GLYPH_HEIGHT = 16
BYTES_PER_GLYPH = 32  # 16 rows * 2 bytes = 32 bytes

def find_font():
    for p in FONT_CANDIDATES:
        if os.path.exists(p):
            return p
    return None

def generate_font(font_path, output_path, ttc_index=7):
    print(f"Loading font: {font_path} (TTC index: {ttc_index})")
    font = ImageFont.truetype(font_path, 16, index=ttc_index)
    
    img = Image.new("1", (16, 16), 0)
    draw = ImageDraw.Draw(img)
    
    num_blocks = len(BLOCKS)
    header_size = 16
    block_entry_size = 16
    total_header_size = header_size + num_blocks * block_entry_size
    
    # Calculate file offsets for each block
    block_records = []
    current_offset = total_header_size
    total_glyphs = 0
    
    for name, start_cp, end_cp, width in BLOCKS:
        count = (end_cp - start_cp) + 1
        block_records.append({
            "name": name,
            "start_cp": start_cp,
            "end_cp": end_cp,
            "width": width,
            "count": count,
            "file_offset": current_offset,
        })
        current_offset += count * BYTES_PER_GLYPH
        total_glyphs += count

    print(f"Total blocks: {num_blocks}")
    print(f"Total glyphs to render: {total_glyphs}")
    print(f"Expected file size: {current_offset} bytes ({current_offset / (1024 * 1024):.2f} MB)")
    
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    
    t0 = time.time()
    with open(output_path, "wb") as f:
        # Write File Header:
        # magic(4B) + version(2B) + glyph_width(1B) + glyph_height(1B) + bytes_per_glyph(1B) + num_blocks(1B) + reserved(6B) = 16B
        header = struct.pack(
            "<4sHBBBB6s",
            MAGIC,
            VERSION,
            GLYPH_WIDTH,
            GLYPH_HEIGHT,
            BYTES_PER_GLYPH,
            num_blocks,
            b"\x00" * 6
        )
        f.write(header)
        
        # Write Block Descriptors:
        # start_cp(4B) + end_cp(4B) + file_offset(4B) + default_width(1B) + reserved(3B) = 16B
        for b in block_records:
            block_data = struct.pack(
                "<III B3s",
                b["start_cp"],
                b["end_cp"],
                b["file_offset"],
                b["width"],
                b"\x00" * 3
            )
            f.write(block_data)
            
        assert f.tell() == total_header_size, f"Header size mismatch: {f.tell()} vs {total_header_size}"
        
        # Render glyphs
        rendered = 0
        for b in block_records:
            print(f"Rendering {b['name']} (U+{b['start_cp']:04X}..U+{b['end_cp']:04X}, {b['count']} glyphs)...")
            chunk_bytes = bytearray()
            for cp in range(b["start_cp"], b["end_cp"] + 1):
                ch = chr(cp)
                draw.rectangle((0, 0, 15, 15), fill=0)
                try:
                    draw.text((0, -5), ch, font=font, fill=1)
                except Exception:
                    pass
                chunk_bytes.extend(img.tobytes())
                rendered += 1
                
                # Flush in chunks to keep memory usage low
                if len(chunk_bytes) >= 64 * 1024:
                    f.write(chunk_bytes)
                    chunk_bytes = bytearray()
                    
            if chunk_bytes:
                f.write(chunk_bytes)
                
    t1 = time.time()
    file_size = os.path.getsize(output_path)
    print(f"Successfully generated {output_path} in {t1 - t0:.2f} seconds.")
    print(f"Final file size: {file_size} bytes ({file_size / (1024 * 1024):.2f} MB)")

if __name__ == "__main__":
    font_file = find_font()
    if not font_file:
        print("Error: Could not find NotoSansCJK font file.")
        sys.exit(1)
        
    out_file = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "noto_cjk_16.bin")
    if len(sys.argv) > 1:
        out_file = sys.argv[1]
        
    generate_font(font_file, out_file)
