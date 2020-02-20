import numpy as np
import PIL.Image
import PIL.ImageChops
import sys

with open(sys.argv[1], 'rb') as f:
    data = f.read()
size = len(data)

blocksize = 104
blocks = size // blocksize
height = 36
width = 35
margin = 2

dots = np.zeros((width * blocks + margin * 2 + 1, height + margin * 2), dtype=np.bool)
anchor = np.array([[0, 1, 1, 1, 0],
                   [1, 1, 1, 1, 1],
                   [1, 1, 1, 1, 1],
                   [1, 1, 1, 1, 1],
                   [0, 1, 1, 1, 0]], dtype=np.bool)
alignment = np.array([1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0], dtype=np.bool)
nybbles = [[0, 0, 0, 0, 0], [0, 0, 0, 0, 1], [0, 0, 0, 1, 0], [1, 0, 0, 1, 0],
           [0, 0, 1, 0, 0], [0, 0, 1, 0, 1], [0, 0, 1, 1, 0], [1, 0, 1, 1, 0],
           [0, 1, 0, 0, 0], [0, 1, 0, 0, 1], [0, 1, 0, 1, 0], [1, 0, 1, 0, 0],
           [0, 1, 1, 0, 0], [0, 1, 1, 0, 1], [1, 0, 0, 0, 1], [1, 0, 0, 0, 0]]
addr = [0x03FF]
for i in range(1, 54):
    addr.append(addr[i - 1] ^ ((i & -i) * 0x769))
    if (i & 0x07) == 0:
        addr[i] ^= 0x769
    if (i & 0x0F) == 0:
        addr[i] ^= 0x769 << 1
    if (i & 0x1F) == 0:
        addr[i] ^= (0x769 << 2) ^ 0x769

base = 1 if blocks == 18 else 25
for i in range(blocks + 1):
    dots[i * width:i * width + 5, 0:5] = anchor
    dots[i * width:i * width + 5, height + margin * 2 - 5:height + margin * 2] = anchor
    dots[i * width + margin, margin + 5] = 1
    a = addr[base + i]
    for j in range(16):
        dots[i * width + margin, margin + 14 + j] = a & (1 << (15 - j))
for i in range(blocks):
    dots[i * width:(i + 1) * width, margin] = alignment
    dots[i * width:(i + 1) * width, height + margin - 1] = alignment
    block = []
    for byte in data[i * blocksize:(i + 1) * blocksize]:
        block.extend(nybbles[byte >> 4])
        block.extend(nybbles[byte & 0xF])
    j = 0
    for y in range(3):
        dots[i * width + margin + 5:i * width + margin + 31, margin + 2 + y] = block[j:j + 26]
        j += 26
    for y in range(26):
        dots[i * width + margin + 1:i * width + margin + 35, margin + 5 + y] = block[j:j + 34]
        j += 34
    for y in range(3):
        dots[i * width + margin + 5:i * width + margin + 31, margin + 31 + y] = block[j:j + 26]
        j += 26
im = PIL.Image.fromarray(dots.T)
im = PIL.ImageChops.invert(im)
im.save('dotcode.png')
