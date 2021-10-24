"""
Module for creating and loading savestates.
"""


def save_to_slot(slot: int, /):
    """
    Saves a savestate to the given slot.
    The slot number must be between 0 and 99.
    """


def save_to_file(filename: str, /):
    """
    Saves a savestate to the given file.
    """


def save_to_bytes() -> bytes:
    """
    Saves a savestate and returns it as bytes.
    """


def load_from_slot(slot: int, /):
    """
    Loads a savestate from the given slot.
    The slot number must be between 0 and 99.
    """


def load_from_file(filename: str, /):
    """
    Loads a savestate from the given file.
    """


def load_from_bytes(state_bytes: bytes, /):
    """
    Loads a savestate from the given bytes.
    """
