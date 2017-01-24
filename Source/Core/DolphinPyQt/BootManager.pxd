from libcpp cimport bool
from libcpp.string cimport string

cdef extern from "Core/BootManager.h" namespace "BootManager":
    bool BootCore(string filename)
    void Stop()
