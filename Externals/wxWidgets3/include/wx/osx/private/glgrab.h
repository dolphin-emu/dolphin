#include <CoreFoundation/CoreFoundation.h>

#if defined __cplusplus
    extern "C" {
#endif

    CGImageRef grabViaOpenGL(CGDirectDisplayID display,
                             CGRect srcRect);

#if defined __cplusplus
    }
#endif

