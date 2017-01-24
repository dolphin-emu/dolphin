from libcpp.string cimport string

cdef extern from "UICommon/UICommon.h" namespace "UICommon":
    void SetUserDirectory(string custom_path)
    void CreateDirectories()
    void Init()
    void Shutdown()
    
