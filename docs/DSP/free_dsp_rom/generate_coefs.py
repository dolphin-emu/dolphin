from numpy import *
from struct import pack

def convert_coefs(c):
    cw = list(zip(c[   :128][::-1],
                  c[128:256][::-1],
                  c[256:384][::-1],
                  c[384:   ][::-1]))
    m = max(sum(x) for x in cw)
    return [int(round(n / m * 32767)) & 0xffff for x in cw for n in x]

def pack_coefs(short_coefs):
    return b''.join(pack('>H', c) for c in short_coefs)

x = linspace(-2, 2, 512, endpoint=True)

w1 = hamming(512)
w2 = kaiser(512, pi * 9/4)

coef_1 = [sinc(n * 0.5)  for n in x] * w1
coef_2 = [sinc(n * 0.75) for n in x] * w2
coef_3 = [sinc(n)        for n in x] * w1

short_coefs = convert_coefs(coef_1) + convert_coefs(coef_2) + convert_coefs(coef_3) + [0] * 512

# needed for GBA ucode
gba_coefs = (
    (0x03b, 0x0065),
    (0x043, 0x0076),
    (0x0ca, 0x3461),
    (0x0e2, 0x376f),
    (0x1b8, 0x007f),
    (0x1b8, 0x007f),
    (0x1f8, 0x0009),
    (0x1fc, 0x0003),
    (0x229, 0x657c),
    (0x231, 0x64fc),
    (0x259, 0x6143),
    (0x285, 0x5aff),
    (0x456, 0x102f),
    (0x468, 0xf808),
    (0x491, 0x6a0f),
    (0x5f1, 0x0200),
    (0x5f6, 0x7f65),
    (0x65b, 0x0000),
    (0x66b, 0x0000),
    (0x66c, 0x06f2),
    (0x6fe, 0x0008),
    (0x723, 0xffe0),
    (0x766, 0x0273),
)
for (addr, value) in gba_coefs:
    old_value = short_coefs[addr]
    if old_value != value:
        print("At %04x: replacing %04x with %04x (diff. of % #x)" % (addr, old_value, value, value - old_value))
        short_coefs[addr] = value

with open('dsp_coef.bin', 'wb') as f:
    f.write(pack_coefs(short_coefs))
