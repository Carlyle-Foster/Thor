#ifndef THOR_INFO_H
#define THOR_INFO_H

// Work out the compiler being used.
#if defined(__clang__)
	#define THOR_COMPILER_CLANG
#elif defined(__GNUC__) || defined(__GNUG__)
	#define THOR_COMPILER_GCC
#elif defined(_MSC_VER)
	#define THOR_COMPILER_MSVC
#else
	#error Unsupported compiler
#endif

// Work out the host platform being targeted.
#if defined(_WIN32)
	#define THOR_HOST_PLATFORM_WINDOWS
#elif defined(__linux__)
	#define THOR_HOST_PLATFORM_LINUX
	#define THOR_HOST_PLATFORM_POSIX
#elif defined(__APPLE__) && defined(__MACH__)
	#define THOR_HOST_PLATFORM_MACOS
	#define THOR_HOST_PLATFORM_POSIX
#else
	#error Unsupported platform
#endif

#if defined(THOR_COMPILER_CLANG) || defined(THOR_COMPILER_GCC)
	#define THOR_FORCEINLINE __attribute__((__always_inline__)) inline
#elif defined(THOR_COMPILER_MSVC)
	#define THOR_FORCEINLINE __forceinline
#endif

#if defined(__has_builtin)
	#define THOR_HAS_BUILTIN(...) __has_builtin(__VA_ARGS__)
#else
	#define THOR_HAS_BUILTIN(...) 0
#endif

#if defined(__has_feature)
	#define THOR_HAS_FEATURE(...) __has_feature(__VA_ARGS__)
#else
	#define THOR_HAS_FEATURE(...) 0
#endif

#if defined(__has_include)
	#define THOR_HAS_INCLUDE(...) __has_include(__VA_ARGS__)
#else
	#define THOR_HAS_INCLUDE(...) 0
#endif

// These are debug build options
// #define THOR_CFG_USE_MALLOC 1

#endif // THOR_INFO_H