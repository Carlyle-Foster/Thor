#include "util/system.h"
#include "util/file.h"
#include "util/map.h"


namespace Thor {
	extern const Filesystem STD_FILESYSTEM;
	extern const Heap       STD_HEAP;
	extern const Console    STD_CONSOLE;
}

using namespace Thor;

#include <stdio.h>

int main(int argc, char **argv) {
	System sys {
		STD_FILESYSTEM,
		STD_HEAP,
		STD_CONSOLE
	};



}