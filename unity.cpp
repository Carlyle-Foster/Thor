// Unity builds: To build just run:
//
// MSVC-like compilers (cl.exe, clang-cl, etc)
// 	cl.exe /nologo /I src /std:c++20 /GR- /unity.cpp /link /out:thor.exe
//
// GNU-like compilers (gcc, clang, etc)
// 	cc -xc++ -Isrc -std=c++20 -fno-rtti -fno-exceptions unity.cpp -o thor
//
// Thor will not link without the use of -fno-rtii and -fno-exceptions.
#include "src/util/allocator.cpp"
#include "src/util/assert.cpp"
#include "src/util/cpprt.cpp"
#include "src/util/file.cpp"
#include "src/util/lock.cpp"
#include "src/util/pool.cpp"
#include "src/util/slab.cpp"
#include "src/util/stream.cpp"
#include "src/util/string.cpp"
#include "src/util/thread.cpp"
#include "src/util/time.cpp"
#include "src/util/unicode.cpp"
#include "src/ast.cpp"
#include "src/lexer.cpp"
#include "src/main.cpp"
#include "src/parser.cpp"
#include "src/cg_llvm.cpp"
#include "src/system_posix.cpp"
#include "src/system_windows.cpp"
