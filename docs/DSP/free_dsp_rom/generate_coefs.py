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

x = linspace(-2, 2, 512, endpoint=False)

w1 = hamming(512)
w2 = kaiser(512, pi * 9/4)

coef_1 = [sinc(n * 0.5)  for n in x] * w1
coef_2 = [sinc(n * 0.75) for n in x] * w2
coef_3 = [sinc(n)        for n in x] * w1

short_coefs = convert_coefs(coef_1) + convert_coefs(coef_2) + convert_coefs(coef_3) + [0] * 512

with open('dsp_coef.bin', 'wb') as f:
    f.write(pack_coefs(short_coefs))
