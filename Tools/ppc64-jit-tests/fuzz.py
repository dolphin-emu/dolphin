#!/usr/bin/env python3
# Runs fuzz.elf on the interpreter and on JITPPC64 at each Power ISA level, and reduces every
# program they run differently to the fewest instructions that still differ (the others replaced
# with nop), which it reports with their disassembly and the registers and memory that differ.
#
#   fuzz.py <dolphin-emu-nogui> <dir with run.sh and fuzz.elf> <out dir> [options]
import argparse
import concurrent.futures
import os
import re
import resource
import struct
import subprocess
import sys
import tempfile

MAGIC = struct.pack(">II", 0x46555A5A, 0x434F4E46)
MAX_LENGTH = 128

parser = argparse.ArgumentParser()
parser.add_argument("dolphin")
parser.add_argument("tests")
parser.add_argument("out")
parser.add_argument("--start", type=int, default=0)
parser.add_argument("--count", type=int, default=2000, help="programs per run")
parser.add_argument("--runs", type=int, default=1)
parser.add_argument("--levels", nargs="+", default=["3.1", "3.0", "2.07"])
parser.add_argument("--max-reduce", type=int, default=20, help="mismatches to reduce per run")
parser.add_argument("--jobs", type=int, default=os.cpu_count())
parser.add_argument("--timeout", type=int, default=600)
parser.add_argument("--option", action="append", default=[],
                    help="a Dolphin setting for both cores, as -C takes it")
parser.add_argument("--mmu", action="store_true", help="MMU mode, with accesses to paged memory")
parser.add_argument("--fprf", action="store_true", help="with FPRF, which is compared too")
parser.add_argument("--no-blr", action="store_true", help="without the JIT's BLR optimization")
parser.add_argument("--env", action="append", default=[], help="NAME=VALUE for the JIT's runs")
args = parser.parse_args()
os.makedirs(args.out, exist_ok=True)
flags = (1 if args.mmu else 0) | (2 if args.fprf else 0)
if args.mmu:
    args.option.append("Dolphin.Core.MMU=True")
if args.fprf:
    args.option.append("Dolphin.Core.FPRF=True")
elf = open(os.path.join(args.tests, "fuzz.elf"), "rb").read()
config_offset = elf.index(MAGIC)


def patched(start, count, dump=False, nop=0, reduced=0xFFFFFFFF):
    words = [start, count, int(dump), reduced, flags] + [(nop >> (32 * i)) & 0xFFFFFFFF
                                                          for i in range(MAX_LENGTH // 32)]
    data = bytearray(elf)
    data[config_offset + 8:config_offset + 8 + 4 * len(words)] = struct.pack(f">{len(words)}I", *words)
    f = tempfile.NamedTemporaryFile(suffix=".elf", delete=False)
    f.write(data)
    f.close()
    return f.name


def no_core_dump():
    # A crash would dump the 18 GiB fastmem arena.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run(path, core, level=None, timeout=None):
    env = dict(os.environ)
    if not args.no_blr:
        env["DOLPHIN_PPC64_BLR_OPTIMIZATION"] = "1"
    if level:
        env["DOLPHIN_PPC64_ISA"] = level
        env.update(e.split("=", 1) for e in args.env)
    out = subprocess.run(["bash", os.path.join(args.tests, "run.sh"), args.dolphin, path, str(core),
                          str(timeout or args.timeout)] +
                         [a for o in args.option for a in ("-C", o)],
                         capture_output=True, text=True, env=env, preexec_fn=no_core_dump).stdout
    if "TEST done" not in out:
        return None, out
    return out, out


def hashes(out):
    return {int(m.group(1)): m.group(2) for m in re.finditer(r"^TEST (\d+) ([0-9a-f]{8})$", out, re.M)}


pool = concurrent.futures.ThreadPoolExecutor(args.jobs)


def differs(p, level, nop, context=0):
    path = patched(p - context, context + 1, nop=nop, reduced=p)
    try:
        a, b = pool.map(lambda c: run(path, *c, timeout=60), [(0, None), (6, level)])
    finally:
        os.unlink(path)
    if a[0] is None:
        return False  # The interpreter must run it.
    if b[0] is None:
        return True  # A crash or hang of the JIT counts.
    return hashes(a[0]).get(p) != hashes(b[0]).get(p)


def reduce(p, level, context):
    # The program's length, from the interpreter's dump.
    path = patched(p, 1, dump=True)
    out, _ = run(path, 0)
    os.unlink(path)
    length = len(re.findall(r"^TEST code ", out, re.M))
    if differs(p, level, (1 << length) - 1, context):
        return None, length  # It differs without any of the program: the harness does.
    nop = 0
    # ddmin over the instructions still in: try removing chunks, halving the chunk size.
    chunk = max(1, length // 2)
    while True:
        live = [i for i in range(length) if not nop >> i & 1]
        changed = False
        for start in range(0, len(live), chunk):
            trial = nop
            for i in live[start:start + chunk]:
                trial |= 1 << i
            if trial != nop and differs(p, level, trial, context):
                nop = trial
                changed = True
        if not changed:
            if chunk == 1:
                break
            chunk = max(1, chunk // 2)
    return nop, length


def report(p, level, nop, length, context):
    path = patched(p - context, context + 1, dump=True, nop=nop, reduced=p)
    (a, _), (b, braw) = pool.map(lambda c: run(path, *c), [(0, None), (6, level)])
    os.unlink(path)
    # The reduced program's dump is the last one.
    a = a[a.rindex("TEST code 0 "):] if "TEST code 0 " in a else a
    if b:
        b = b[b.rindex("TEST code 0 "):] if "TEST code 0 " in b else b
    words = [int(w, 16) for w in re.findall(r"^TEST code \d+ ([0-9a-f]{8})$", a, re.M)]
    live = [(i, w) for i, w in enumerate(words) if w != 0x60000000]
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        for _, w in live:
            f.write(struct.pack(">I", w))
    dis = subprocess.run(["objdump", "-D", "-b", "binary", "-m", "powerpc:750", "-EB", f.name],
                         capture_output=True, text=True).stdout
    os.unlink(f.name)
    lines = [l.split("\t", 2)[-1] for l in dis.splitlines() if re.match(r"^\s+[0-9a-f]+:\t", l)]
    text = [f"program {p}, ISA {level}: {len(live)} of {length} instructions" +
            (f", after programs {p - context}-{p - 1}" if context else "")]
    text += [f"  {i:3}: {w:08x}  {d}" for (i, w), d in zip(live, lines)]
    text.append("differences (interpreter | JIT):")
    fields = r"^TEST ((?:r|f|m|pte)\d+|p\d+_\d+|cr|exceptions) (.*)$"
    fa = dict(re.findall(fields, a, re.M))
    fb = dict(re.findall(fields, b or braw, re.M)) if b else {}
    if not b:
        text.append("  JIT run failed:\n" + braw[-2000:])
    for k in fa:
        if fa[k] != fb.get(k):
            text.append(f"  {k}: {fa[k]} | {fb.get(k)}")
    return "\n".join(text)


for r in range(args.runs):
    start = args.start + r * args.count
    path = patched(start, args.count)
    jobs = [(0, None)] + [(6, level) for level in args.levels]
    results = list(pool.map(lambda c: run(path, *c), jobs))
    os.unlink(path)
    ref, _ = results[0]
    if ref is None:
        print(f"run {start}: the interpreter didn't finish", flush=True)
        continue
    ref = hashes(ref)
    summary = []
    found = []
    for level, (out, raw) in zip(args.levels, results[1:]):
        if out is None:
            # The JIT crashed or hung in the program after the last it reported.
            done = hashes(raw)
            p = max(done) + 1 if done else start
            summary.append(f"{level}:FAILED at {p}")
            open(os.path.join(args.out, f"failed-{start}-{level}.log"), "w").write(raw)
            found.append((p, level))
            continue
        h = hashes(out)
        bad = sorted(p for p in ref if h.get(p) != ref[p])
        summary.append(f"{level}:{len(bad)}")
        found += [(p, level) for p in bad]
    print(f"programs {start}-{start + args.count - 1}: {len(ref)} run, mismatches " +
          " ".join(summary), flush=True)
    seen = set()
    for p, level in found:
        if p in seen or len(seen) >= args.max_reduce:
            continue
        seen.add(p)
        # Alone, or else after the programs before it.
        context = next((c for c in (0, 1, 8, p - start) if c <= p - start and
                        differs(p, level, 0, c)), None)
        if context is None:
            print(f"program {p}, ISA {level}: doesn't differ again", flush=True)
            continue
        nop, length = reduce(p, level, context)
        if nop is None:
            text = f"program {p}, ISA {level}: differs with all of it replaced by nop, in the harness"
        else:
            text = report(p, level, nop, length, context)
        open(os.path.join(args.out, f"program-{p}.txt"), "w").write(text + "\n")
        print(text, flush=True)
