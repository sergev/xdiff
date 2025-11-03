# Tests CMakeLists.txt
# Separate CMake script for test configuration

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)

# Disable GoogleMock
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
# Don't install GoogleTest
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Configure test config header with build directory path
# Use parent binary directory (CMAKE_BINARY_DIR) for the build path
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/test_config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/test_config.h
  @ONLY
)

# Create test executable
add_executable(test_xdiff_cli test_xdiff_cli.cpp)
target_link_libraries(test_xdiff_cli PRIVATE gtest_main)

# Set C++17 standard for tests
set_target_properties(test_xdiff_cli PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# Link filesystem library if needed (for GCC)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(test_xdiff_cli PRIVATE stdc++fs)
endif()

# Include generated config header
target_include_directories(test_xdiff_cli PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Include GoogleTest module (must be after target is created)
include(GoogleTest)

# Register tests with CTest using gtest_discover_tests
gtest_discover_tests(test_xdiff_cli EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 5)
