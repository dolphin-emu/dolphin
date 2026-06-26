# Type stub for the embedded `dolphin_dtm` module (import as: import dolphin_dtm as dtm).
# Inspect and edit the live DTM movie row-by-row. Setters return False when the
# row is out of range or otherwise rejected, True on success.

from typing import Any, Dict, Optional

# Metadata dict has both PascalCase and snake_case keys: Kind/kind ('gc'|'wii'),
# RowCount, CurrentFrame, CurrentInputRow, DataGeneration, IsRecording,
# IsPlaying, IsReadOnly, plus ActiveControllers/active_controllers (gc) or
# ActiveWiimotes/active_wiimotes (wii). None when no movie is active.
def get_metadata() -> Optional[Dict[str, Any]]: ...
def get_kind() -> Optional[str]: ...            # 'gc', 'wii', or None

# GC row state mirrors controller.get_gc_buttons keys. None if row missing.
def get_gc(row: int, controller: int) -> Optional[Dict[str, Any]]: ...
def set_gc(row: int, controller: int, state: Dict[str, Any]) -> bool: ...  # partial update

# Wii row dict: Wiimote (int), Reset (bool), SerializedState (bytes),
# Buttons (dict, absent on reset rows); snake_case aliases included.
def get_wii(row: int) -> Optional[Dict[str, Any]]: ...
def get_wii_buttons(row: int) -> Optional[Dict[str, Any]]: ...
def set_wii_buttons(row: int, state: Dict[str, Any]) -> bool: ...
def set_wii_serialized(row: int, data: bytes) -> bool: ...  # data at most 31 bytes
