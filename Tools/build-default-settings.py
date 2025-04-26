#!/usr/bin/env python3

# We have over 1700 ini files now.
# Extracting/updating them all is very slow on Windows.
# This script compresses them all into a single file during the build.

from glob import glob
from zstandard import ZstdCompressor

raw = b''
for i in glob('*.ini'):
  raw += i.encode('ascii') + b'\0'
  ini = open(i, 'rb').read()
  assert b'\0' not in ini
  raw += ini + b'\0'

cooked = ZstdCompressor(19).compress(raw)
open('default.bin.zstd', 'wb+').write(cooked)
