# About
This project is MIT-licensed, C++14 implementation of [semantic versioning](http://semver.org) parser and comparator with support for modifying parsed version strings. Semantic versioning 2.0.0 specification is supported out-of-the-box and the code should be flexible-enough to support future revisions or other similar versioning schemes.

# Usage
Parsing and comparing two version strings:
```c++
#include "semver200.h"

void main(int, char**) {
    version::Semver200_version v1("1.0.0");
    version::Semver200_version v2("2.0.0");

    if (v2 > v1) {
        std::cout << v2 << " is indeed greater than " << v1 << std::endl;
    } else {
        std::cout << "This thing is broken, what a waste of time!" << std::endl;
    }
}
```

Accessing individual version components:

```c++
#include "semver200.h"

void main(int, char**) {
    version::Semver200_version v("1.2.3-alpha.1+build.no.123");
    std::cout << "Major: " << v.major() << std::endl;
    std::cout << "Minor: " << v.minor() << std::endl;
    std::cout << "Patch: " << v.patch() << std::endl;
    std::cout << "Pre-release: " << v.prerelease() << std::endl;
    std::cout << "Build: " << v.build() << std::endl;
}
```

should generate following output:
```
Major: 1
Minor: 2
Patch: 3
Pre-release: alpha.1
Build: build.no.123
```

Parsed version object supports a few modification methods. All modification methods are non-destructive i.e. they return new objects with modified properties and original objects are never changed. You can:

- set major, minor, patch, pre-release or build version to desired value while keeping other fields unchanged;
- reset major, minor, patch, pre-release or build version to desired value by resetting lower-priority fields to zero/empty values (Priority is major > minor > patch > pre-release > build);
- increase/decrease major, minor or patch version by desired increment (can be negative); this is reset-type operation which will zero/empty lower priority fields.

A few examples of version modifications:

```c++
#include "semver200.h"

void main(int, char**) {
    version::Semver200_version v("1.2.3-alpha.1+build.no.123");
    std::cout << "Next major version: " << v.inc_major() << std::endl;
    std::cout << "Next but one major: " << v.inc_major(2) << std::endl;
    std::cout << "Next minor version: " << v.inc_minor() << std::endl;
    std::cout << "Previous minor version: " << v.inc_minor(-1) << std::endl;
    std::cout << "Next patch version: " << v.inc_patch() << std::endl;
    std::cout << "Change pre-release: " << v.set_prerelease("beta.3") << std::endl;
    std::cout << "Change build: " << v.set_build("170105") << std::endl;
    std::cout << "Set major to 3, minor to 1: " << v.set_major(3).set_minor(1) << std::endl;
    std::cout << "Reset major to 3, minor to 1: " << v.reset_major(3).reset_minor(1) << std::endl;
}
```

should generate following output:
```
Next major version: 2.0.0
Next but one major: 3.0.0
Next minor version: 1.3.0
Previous minor version: 1.1.0
Next patch version: 1.2.4
Change pre-release: 1.2.3-beta.3+build.no.123
Change build: 1.2.3-alpha.1+170105
Set major to 3, minor to 1: 3.1.3-alpha.1+build.no.123
Reset major to 3, minor to 1: 3.1.0
```

# Build
The code is written in C++14, so, fairly recent compiler is required to build it. Following compilers were tested:
- Microsoft Visual Studio 2015
- GCC 5.1.1
- Clang 3.7.0

Library itself does not have any external dependencies. Unit tests that verify the library work as expected, on the other hand, depend on the [Boost.Test](http://www.boost.org/doc/libs/1_59_0/libs/test/doc/html/index.html) library. Unit tests are disabled by default. To build tests run cmake with -DSEMVER_ENABLE_TESTING=ON option.

The code comes with CMake project files. In order to build it you should:

- create, if it doesn’t already exist, directory `build` in the project directory;
- invoke `cmake ..` in build directory;
- once CMake is done, use your toolset (Visual Studio, nmake, make, …) to build the library;
- remember to link in the library you have built and to include `./include` directory to your build.
