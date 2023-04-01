from numpy import *
from struct import pack

def pack_coefs(c):
    cw = list(zip(c[   :128][::-1],
                  c[128:256][::-1],
                  c[256:384][::-1],
                  c[384:   ][::-1]))
    m = max(sum(x) for x in cw)
    return b''.join(pack('>4h', *(int(round(n / m * 32767)) for n in x)) for x in cw)

x = linspace(-2, 2, 512, endpoint=False)

w1 = hamming(512)
w2 = kaiser(512, pi * 9/4)

coef_1 = [sinc(n * 0.5)  for n in x] * w1
coef_2 = [sinc(n * 0.75) for n in x] * w2
coef_3 = [sinc(n)        for n in x] * w1

with open('dsp_coef.bin', 'wb') as f:
    f.write(pack_coefs(coef_1))
    f.write(pack_coefs(coef_2))
    f.write(pack_coefs(coef_3))
    f.write(b'\0' * 1024)
