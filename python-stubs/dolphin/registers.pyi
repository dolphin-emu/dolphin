# Type stub for the embedded `dolphin.registers` module.
# PowerPC register access. index is 0..31; out-of-range raises ValueError.

def read_gpr(index: int) -> int: ...           # general-purpose register r0..r31
def write_gpr(index: int, value: int) -> None: ...
def read_fpr(index: int) -> float: ...          # floating-point register f0..f31
def write_fpr(index: int, value: float) -> None: ...
