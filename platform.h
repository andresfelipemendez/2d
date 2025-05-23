#ifndef PLATFORM_H
#define PLATFORM_H

// Platform detection macros
#if defined(__APPLE__) && defined(__MACH__)
    #define PLATFORM_MAC 1
    #if defined(__arm64__) || defined(__aarch64__)
        #define PLATFORM_MAC_ARM 1
    #else
        #define PLATFORM_MAC_X64 1
    #endif
#elif defined(__linux__)
    #define PLATFORM_LINUX 1
    #if defined(__x86_64__)
        #define PLATFORM_LINUX_X64 1
    #elif defined(__aarch64__)
        #define PLATFORM_LINUX_ARM 1
    #endif
#elif defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    #if defined(_WIN64)
        #define PLATFORM_WINDOWS_X64 1
    #else
        #define PLATFORM_WINDOWS_X86 1
    #endif
#endif

// Platform-specific library extensions
#if defined(PLATFORM_MAC)
    #define DYLIB_EXTENSION ".dylib"
#elif defined(PLATFORM_LINUX)
    #define DYLIB_EXTENSION ".so"
#elif defined(PLATFORM_WINDOWS)
    #define DYLIB_EXTENSION ".dll"
#else
    #error "Unsupported platform"
#endif

// Platform-specific compiler
#if defined(PLATFORM_MAC)
    #define COMPILER "clang"
#elif defined(PLATFORM_LINUX)
    #define COMPILER "gcc"
#elif defined(PLATFORM_WINDOWS)
    #define COMPILER "cl"
#endif

// Platform-specific path separator
#if defined(PLATFORM_WINDOWS)
    #define PATH_SEPARATOR "\\"
#else
    #define PATH_SEPARATOR "/"
#endif

// Platform-specific temporary directory
#if defined(PLATFORM_MAC)
    #define TEMP_DIR "./"  // Current directory to avoid permission issues
#elif defined(PLATFORM_LINUX)
    #define TEMP_DIR "/tmp/"
#elif defined(PLATFORM_WINDOWS)
    #define TEMP_DIR "./"  // Or use GetTempPath() for proper temp directory
#endif

// Platform name string for display
#if defined(PLATFORM_MAC_ARM)
    #define PLATFORM_NAME "macOS ARM64 (Apple Silicon)"
#elif defined(PLATFORM_MAC_X64)
    #define PLATFORM_NAME "macOS x86_64"
#elif defined(PLATFORM_LINUX_X64)
    #define PLATFORM_NAME "Linux x86_64"
#elif defined(PLATFORM_LINUX_ARM)
    #define PLATFORM_NAME "Linux ARM64"
#elif defined(PLATFORM_WINDOWS_X64)
    #define PLATFORM_NAME "Windows x64"
#elif defined(PLATFORM_WINDOWS_X86)
    #define PLATFORM_NAME "Windows x86"
#else
    #define PLATFORM_NAME "Unknown Platform"
#endif

#endif // PLATFORM_H