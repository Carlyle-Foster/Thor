# Thor (A new compiler for Odin)
## Goals
* 4 GiB/s of compile speed [ONTARGET] (we are already 6 GiB/s /w 32 threads)
  * Including linking time!
* Deterministic builds
  * Same input produces the same executable
    * Same MD5SUM / SHA1 hashes
* Incremental builds (rebuild what has changed)
  * Serialized AST [DONE]
    * Slab -> Cache -> Ref
  * Serialized IR
    * Slab -> Cache -> Ref
* Data oriented [DONE]
  * No pointers, just indices and parallel arrays for the AST/IR
* Fast builds
  * Fully threaded and out-of-order
    * Build an object as soon as possible
      * When a function is _complete_ it can be generated
* Employ a middle IR (MIR)
  * Compile to C
  * Compile to LLVM
* Monomorphization at the AST
  * Fixing all the issues with current Odin Parapoly
    * Global monomorphization cache
      * To reduce code bloat
* Better debug support
  * Said said he would help with this.
* Smaller executables
  * Static type information (no initialization needed @ runtime)
* Smaller compiler codebase [ONTARGET]
  * Heavy code reuse and abstractions
    * deducing-this, concepts, etc
* Compiler can be incrementally rebuilt and built fast [DONE]
  * Optional unity builds (every source file included in a `unity.cpp`)
* Only fast linker support.
  * Only radlink on Windows
  * Only mold on Linux
  * LTO support

# Architecture
* Modern C++23
  * No template metaprogramming nonsense
    * Will only use templates for containers and varadic packs
    * Will use concepts though
    * Will use lambdas though
* Monadic (Maybe, Mutex, et al) [ONTARGET]
  * Less error prone
* Table driven [DONE]
  * Lexer and parser
* No implicit copies (all explicit) [ONTARGET]
  * Every struct has copy assignment and copy constructor deleted at all times.
* Extensive use of move semantics for containers
* Multiple allocators
  * Temporary [DONE]
  * Scratch [DONE]
  * Permanent
* No memory leaks (not even on exit) [ONTARGET]
* No data races
* Avoid the use of hash tables unless necessary
  * Flat arrays that are simply indexed, that is so much faster.


# Non-goals
* Architectures other than `amd64` and `aarch64`
  * So size_of(int) = 8, size_of(ptr) = 8 always basically...
* Operating systems other than macOS, Windows and Linux
* `Odin vet`
* Documentation generation ala `Odin doc`
* ABI compatability with existing Odin