### Introduction

OpenXR is an interface defined by Khronos Group. Implementation of the interface is done by each VR headset vendor differently.

### Files

The used interface (headers in C language) is an older version because Pico doesn't support the latest one yet.
The Implementations are taken from the vendor specific SDKs.

`libopenxr_loader.so` in `stub` is there only to satisfy linker, it doesn't matter which variant is it.

### Links

- Khronos OpenXR SDK: https://github.com/KhronosGroup/OpenXR-SDK/
- Oculus OpenXR SDK: https://developer.oculus.com/downloads/package/oculus-openxr-mobile-sdk/
- Pico OpenXR SDK https://developer-global.pico-interactive.com/sdk?deviceId=1&platformId=3&itemId=11
