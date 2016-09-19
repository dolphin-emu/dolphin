cdef extern from "Core/Core.h" namespace "Core":
    enum EState:
        CORE_UNINITIALIZED
        CORE_PAUSE
        CORE_RUN
        CORE_STOPPING

    EState GetState()
    void SetState(EState state)

    void HostDispatchJobs()
    void Shutdown()
    void SaveScreenShot()     