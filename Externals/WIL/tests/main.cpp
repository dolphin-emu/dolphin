
#pragma comment(lib, "Pathcch.lib")
#pragma comment(lib, "RuntimeObject.lib")
#pragma comment(lib, "Synchronization.lib")
#pragma comment(lib, "RpcRt4.lib")

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#if WITEST_ADDRESS_SANITIZER
extern "C" __declspec(dllexport)
const char* __asan_default_options()
{
    return
        // Tests validate OOM, so this is expected
        "allocator_may_return_null=1"
        // Some structs in Windows have dynamic size where we over-allocate for extra data past the end
        ":new_delete_type_mismatch=0";
}
#endif
