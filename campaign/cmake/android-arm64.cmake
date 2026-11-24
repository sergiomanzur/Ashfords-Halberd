# Cross-build the arm64-v8a tests-only campaign with the Android NDK.
#   cmake -B build-android -G Ninja -DCMAKE_TOOLCHAIN_FILE=campaign/cmake/android-arm64.cmake -DANDROID_NDK=<ndk>
# The decomp's GCC-layout parity (32-byte aligned arrays, etc.) is handled by
# explicit alignment in the port, not by the compiler; this toolchain just
# selects the NDK's clang/llvm for arm64 Android.
set(ANDROID_ABI "arm64-v8a")
set(ANDROID_PLATFORM "android-26")
if(NOT ANDROID_NDK AND DEFINED ENV{ANDROID_NDK})
    set(ANDROID_NDK "$ENV{ANDROID_NDK}")
endif()
if(NOT ANDROID_NDK)
    message(FATAL_ERROR "ANDROID_NDK is not set. Pass -DANDROID_NDK=<ndk-root> to cmake.")
endif()
include("${ANDROID_NDK}/build/cmake/android.toolchain.cmake")
