# The PCH that dolphin uses for MSVC is non-standard;
# Instead of having one PCH per module, dolphin has one PCH shared between all modules.
# So we need to implement PCH manually, rather than using the PCH support built into cmake

add_library(pch pch.h pch.cpp)

# pch.cpp should be compiled with the /Yc command, which creates the precompiled header
target_compile_options(pch PRIVATE /Yc"pch.h" /Fo$<TARGET_FILE_DIR:pch>)

# /Fp sets the location of the PCH. By forcing it to a fixed location, all modules
# will share this one PCH
target_compile_options(pch PUBLIC /Fp$<TARGET_FILE_DIR:pch>)

# Sharing a PCH breaks pdb files. So we use the /Z7 option to inline the pdb into
# the binary. That also requires us to disable minimal rebuilds.
target_compile_options(pch PUBLIC /Z7 /Gm-)

# targets which include pch need these compile options
# /Yu - Use precompiled header named "pch.h"
# /FI - Force include "pch.h" at top of every source file
target_compile_options(pch INTERFACE /Yu"pch.h" /FI"pch.h")

# fmt/format.h is included in the PCH
target_link_libraries(pch PUBLIC fmt::fmt)
