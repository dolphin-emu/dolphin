# Stub for the `dolphin` aggregator package.
# Mirrors the embedded module that lets scripts do `from dolphin import event, memory`.
# `utils` is the runtime name for the util submodule; dtm is reached via
# `import dolphin_dtm as dtm` but is re-exported here for editor convenience.

from . import controller as controller
from . import debug as debug
from . import dtm as dtm
from . import event as event
from . import gui as gui
from . import memory as memory
from . import registers as registers
from . import savestate as savestate
from . import util as utils

__all__ = [
    "controller",
    "debug",
    "event",
    "gui",
    "memory",
    "registers",
    "savestate",
    "utils",
]
