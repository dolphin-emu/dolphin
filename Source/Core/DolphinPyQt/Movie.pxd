from libcpp cimport bool

cdef extern from "Core/Movie.h" namespace "Movie":
     bool IsRecordingInput()
     void SetReset(bool reset)
     
     void DoFrameStep()