#!/usr/bin/env python3
"""
Build BestTextEditor.emoji, the colour emoji set BestTextEditor.mcc draws.

Input
  - the 72x72 PNGs of a Twemoji release (assets/72x72 of
    https://github.com/jdecked/twemoji, graphics licensed CC-BY 4.0)
  - emoji-data.txt from the Unicode Character Database
    (https://www.unicode.org/Public/<version>/ucd/emoji/emoji-data.txt), for
    the Emoji_Presentation property

Usage
  python3 make-emoji-data.py <assets/72x72> <emoji-data.txt> <output> [size]

Needs Pillow. The default size is 20 pixels; the class scales the emoji to
the height of the font they are drawn in.

File format (all numbers big endian)
  0   'BTEM'
  4   UWORD version (1)
  6   UWORD size of every emoji in pixels (square)
  8   UWORD number of emoji
  10  UWORD number of palette colours n (at most 255)
  12  ULONG bytes of index
  16  ULONG file offset of the pixel data
  20  palette: n * 3 bytes RGB. Pixel value 0 is transparent, value k is
      palette entry k - 1.
      index: one record per emoji, sorted by code point sequence
        UBYTE  number of code points (bits 0-6) and 0x80 when the emoji has
               Emoji_Presentation (only set for single code points)
        3 bytes per code point (U+FE0F variation selectors are left out)
        3 bytes offset of the pixels from the start of the pixel data
  data: per emoji size * size pixel values, run length encoded:
        control byte c; c & 0x80: (c & 0x7f) copies of the next byte;
        otherwise c literal bytes follow
"""
import os
import struct
import sys

from PIL import Image

FE0F = 0xFE0F


def read_presentation(path):
    """Code points with Emoji_Presentation=Yes"""
    cps = set()
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            rng, prop = [x.strip() for x in line.split(';')[:2]]
            if prop != 'Emoji_Presentation':
                continue
            if '..' in rng:
                a, b = rng.split('..')
                cps.update(range(int(a, 16), int(b, 16) + 1))
            else:
                cps.add(int(rng, 16))
    return cps


def rle(pixels):
    out = bytearray()
    i = 0
    n = len(pixels)
    while i < n:
        j = i
        while j < n and pixels[j] == pixels[i] and j - i < 127:
            j += 1
        if j - i >= 3:
            out += bytes((0x80 | (j - i), pixels[i]))
            i = j
            continue
        lit = bytearray()
        k = i
        while k < n and len(lit) < 127:
            r = k
            while r < n and pixels[r] == pixels[k] and r - k < 3:
                r += 1
            if r - k >= 3:
                break
            lit.append(pixels[k])
            k += 1
        out += bytes((len(lit),)) + lit
        i = k
    return bytes(out)


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    src, emoji_data, output = sys.argv[1:4]
    size = int(sys.argv[4]) if len(sys.argv) > 4 else 20
    presentation = read_presentation(emoji_data)

    entries = {}
    for name in os.listdir(src):
        if not name.endswith('.png'):
            continue
        seq = tuple(int(x, 16) for x in name[:-4].split('-'))
        key = tuple(cp for cp in seq if cp != FE0F)
        if not key or len(key) > 127 or max(key) > 0xFFFFFF:
            continue
        # prefer the file whose name had no variation selector
        if key in entries and len(entries[key][0]) <= len(seq):
            continue
        entries[key] = (seq, os.path.join(src, name))
    keys = sorted(entries)

    images = []
    for key in keys:
        im = Image.open(entries[key][1]).convert('RGBA')
        # resize premultiplied so transparent edges do not darken
        im = im.convert('RGBa').resize((size, size), Image.LANCZOS)
        images.append(im.convert('RGBA'))

    # One palette for every emoji, from all of their opaque pixels
    cols = 64
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new('RGB', (size * cols, size * rows), (0, 0, 0))
    for i, im in enumerate(images):
        opaque = Image.new('RGB', im.size, (0, 0, 0))
        opaque.paste(im.convert('RGB'), mask=im.getchannel('A'))
        sheet.paste(opaque, ((i % cols) * size, (i // cols) * size))
    pal_image = sheet.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
    palette = pal_image.getpalette()[:255 * 3]
    ncolors = len(palette) // 3

    index = bytearray()
    data = bytearray()
    for key, im in zip(keys, images):
        q = im.convert('RGB').quantize(palette=pal_image,
                                       dither=Image.Dither.NONE).tobytes()
        alpha = im.getchannel('A').tobytes()
        pixels = bytes(0 if alpha[i] < 128 else q[i] + 1
                       for i in range(len(q)))
        flags = 0x80 if len(key) == 1 and key[0] in presentation else 0
        index.append(len(key) | flags)
        for cp in key:
            index += cp.to_bytes(3, 'big')
        index += len(data).to_bytes(3, 'big')
        data += rle(pixels)

    header_len = 20 + ncolors * 3
    data_offset = header_len + len(index)
    with open(output, 'wb') as f:
        f.write(b'BTEM')
        f.write(struct.pack('>HHHHII', 1, size, len(keys), ncolors,
                            len(index), data_offset))
        f.write(bytes(palette))
        f.write(index)
        f.write(data)
    print('%d emoji, %d colours, %d bytes' %
          (len(keys), ncolors, data_offset + len(data)))


if __name__ == '__main__':
    main()
