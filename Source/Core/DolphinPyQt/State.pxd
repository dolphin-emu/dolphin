from libcpp cimport bool
from libcpp.string cimport string

cdef extern from "Core/State.h" namespace "State":
    void Save(int slot, bool wait)
    void Load(int slot)

    void SaveAs(string filename, bool wait)
    void LoadAs(string filename)

    void SaveFirstSaved()
    void UndoSaveState()
    void UndoLoadState()
    
