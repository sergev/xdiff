PROG            = xdiff

all:            build
		$(MAKE) -C build

build:
		cmake -B build -DCMAKE_BUILD_TYPE=Debug $(CMAKE_ARGS)

install:        all
		cmake --install build

test:           all
		ctest --test-dir build/tests

clean:
		rm -rf build *.gcov

reindent:
		@echo "Running clang-format on C++ sources..."
		@command -v clang-format >/dev/null 2>&1 || { echo "Error: clang-format not found in PATH"; exit 1; }
		@clang-format -i *.h *.c tests/*.cpp
