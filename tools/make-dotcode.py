import numpy as np
import PIL.Image
import PIL.ImageChops
import sys

blocksize = 104
pow = [0] * 256
rev = [0] * 256
rev[0] = 0xFF
x = 1
for i in range(0xFF):
    pow[i]=x
    rev[x]=i
    x *= 2
    if x >= 0x100:
        x ^= 0x187
gg = [0] * 16
gg[0] = pow[0x78]
for i in range(16):
    gg[i] = 1
    for j in range(i + 1):
        j = i - j
        if j == 0:
            y = 0
        else:
            y = gg[j - 1]
        x = gg[j]
        if x != 0:
            x = rev[x] + 0x78 + i
            if x >= 0xFF:
                x -= 0xFF
            y ^= pow[x]
        gg[j] = y
for i in range(16):
    gg[i] = rev[gg[i]]

def interleave(data, header):
    data = data.reshape([-1, 48]).T
    new_data = np.zeros((64, data.shape[1]), dtype=data.dtype)
    for i, row in enumerate(data.T):
        new_data[:,i] = rs(row)
    data = new_data.reshape([-1])
    new_data = np.zeros(data.shape[0] + (102 - data.shape[0] % 102), dtype=data.dtype)
    new_data[:data.shape[0]] = data
    new_data = new_data.reshape([-1, 102])
    data = new_data
    new_data = np.zeros((data.shape[0], 104), dtype=data.dtype)
    new_data[:,2:] = data
    for i in range(new_data.shape[0]):
        x = (i * 2) % len(header)
        new_data[i,:2] = header[x:x + 2]
    data = new_data.reshape([-1])
    return data

def rs(data):
    new_data = np.zeros(data.shape[0] + 16, dtype=data.dtype)
    new_data[:data.shape[0]] = data
    new_data = np.flipud(new_data)
    for i in range(data.shape[0]):
        i = new_data.shape[0] - i - 1
        z = rev[new_data[i] ^ new_data[15]]
        for j in range(16):
            j = 15 - j
            if j == 0:
                x = 0
            else:
                x = new_data[j - 1]
            if z != 0xFF:
                y = gg[j]
                if y != 0xFF:
                    y += z
                    if y >= 0xFF:
                        y -= 0xFF
                    x ^= pow[y]
            new_data[j] = x
    new_data[:16] = ~new_data[:16]
    new_data = np.flipud(new_data)
    return new_data

def bin2raw(data):
    if len(data) == 1344:
        header = np.array([0x00, 0x02, 0x00, 0x01, 0x40, 0x10, 0x00, 0x1C], dtype=np.uint8)
    else:
        header = np.array([0x00, 0x03, 0x00, 0x19, 0x40, 0x10, 0x00, 0x2C], dtype=np.uint8)
    header = rs(header)
    new_data = interleave(np.frombuffer(data, np.uint8), header)
    return new_data.tobytes()

def make_dotcode(data):
    size = len(data)

    if size in (1344, 2112):
        data = bin2raw(data)
        size = len(data)

    blocks = size // blocksize
    height = 36
    width = 35
    margin = 2

    dots = np.zeros((width * blocks + margin * 2 + 1, height + margin * 2), dtype=bool)
    anchor = np.array([[0, 1, 1, 1, 0],
                       [1, 1, 1, 1, 1],
                       [1, 1, 1, 1, 1],
                       [1, 1, 1, 1, 1],
                       [0, 1, 1, 1, 0]], dtype=bool)
    alignment = np.array([1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0], dtype=bool)
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
    return np.pad(dots.T, 2)

with open(sys.argv[1], 'rb') as f:
    dots = make_dotcode(f.read())

im = PIL.Image.fromarray(dots)
im = PIL.ImageChops.invert(im)
im.save('dotcode.bmp')
