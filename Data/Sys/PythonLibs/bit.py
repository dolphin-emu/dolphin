import dolphin
dolphin.importModule("BitAPI", "1.0")
import BitAPI

def bitwise_and(arg1, arg2):
    return BitAPI.bitwise_and(arg1, arg2)

def bitwise_or(arg1, arg2):
    return BitAPI.bitwise_or(arg1, arg2)

def bitwise_not(arg):
    return BitAPI.bitwise_not(arg)

def bitwise_xor(arg1, arg2):
    return BitAPI.bitwise_xor(arg1, arg2)

def logical_and(arg1, arg2):
    return BitAPI.logical_and(arg1, arg2)

def logical_or(arg1, arg2):
    return BitAPI.logical_or(arg1, arg2)

def logical_xor(arg1, arg2):
    return BitAPI.logical_xor(arg1, arg2)

def logical_not(arg):
    return BitAPI.logical_not(arg)

def bit_shift_left(arg1, arg2):
    return BitAPI.bit_shift_left(arg1, arg2)

def bit_shift_right(arg1, arg2):
    return BitAPI.bit_shift_right(arg1, arg2)