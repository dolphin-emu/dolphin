"""
Module for interacting with the emulated machine's memory
"""


def read_u8(addr: int, /) -> int:
    """
    Reads 1 byte as an unsigned integer.

    :param addr: memory address to read from
    :return: value as integer
    """


def read_u16(addr: int, /) -> int:
    """
    Reads 2 bytes as an unsigned integer.

    :param addr: memory address to read from
    :return: value as integer
    """


def read_u32(addr: int, /) -> int:
    """
    Reads 4 bytes as an unsigned integer.

    :param addr: memory address to read from
    :return: value as integer
    """


def read_u64(addr: int, /) -> int:
    """
    Reads 8 bytes as an unsigned integer.

    :param addr: memory address to read from
    :return: value as integer
    """


def read_s8(addr: int, /) -> int:
    """
    Reads 1 byte as a signed integer.

    :param addr: memory address to read from
    :return: value as integer
    """


def read_s16(addr: int, /) -> int:
    """
    Reads 2 bytes as a signed integer.

    :param addr: memory address to read from
    :return: value as integer
    """


def read_s32(addr: int, /) -> int:
    """
    Reads 4 bytes as a signed integer.

    :param addr: memory address to read from
    :return: value as integer
    """


def read_s64(addr: int, /) -> int:
    """
    Reads 8 bytes as a signed integer.

    :param addr: memory address to read from
    :return: value as integer
    """


def read_f32(addr: int, /) -> float:
    """
    Reads 4 bytes as a floating point number.

    :param addr: memory address to read from
    :return: value as floating point number
    """


def read_f64(addr: int, /) -> float:
    """
    Reads 8 bytes as a floating point number.

    :param addr: memory address to read from
    :return: value as floating point number
    """


def write_u8(addr: int, value: int, /):
    """
    Writes an unsigned integer to 1 byte.
    Overflowing values are truncated.

    :param addr: memory address to read from
    :param value: value as integer
    """


def write_u16(addr: int, value: int, /):
    """
    Writes an unsigned integer to 2 bytes.
    Overflowing values are truncated.

    :param addr: memory address to read from
    :param value: value as integer
    """


def write_u32(addr: int, value: int, /):
    """
    Writes an unsigned integer to 4 bytes.
    Overflowing values are truncated.

    :param addr: memory address to read from
    :param value: value as integer
    """


def write_u64(addr: int, value: int, /):
    """
    Writes an unsigned integer to 8 bytes.
    Overflowing values are truncated.

    :param addr: memory address to read from
    :param value: value as integer
    """


def write_s8(addr: int, value: int, /):
    """
    Writes a signed integer to 1 byte.
    Overflowing values are truncated.

    :param addr: memory address to read from
    :param value: value as integer
    """


def write_s16(addr: int, value: int, /):
    """
    Writes a signed integer to 2 bytes.
    Overflowing values are truncated.

    :param addr: memory address to read from
    :param value: value as integer
    """


def write_s32(addr: int, value: int, /):
    """
    Writes a signed integer to 4 bytes.
    Overflowing values are truncated.

    :param addr: memory address to read from
    :param value: value as integer
    """


def write_s64(addr: int, value: int, /):
    """
    Writes a signed integer to 8 bytes.
    Overflowing values are truncated.

    :param addr: memory address to read from
    :param value: value as integer
    """


def write_f32(addr: int, value: float, /):
    """
    Writes a floating point number to 4 bytes.
    Overflowing values are truncated.

    :param addr: memory address to read from
    :param value: value as floating point number
    """


def write_f64(addr: int, value: float, /):
    """
    Writes a floating point number to 8 bytes.
    Overflowing values are truncated.

    :param addr: memory address to read from
    :param value: value as floating point number
    """
