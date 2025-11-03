cmake_minimum_required(VERSION 3.10)

project(xdiff VERSION "0.1" LANGUAGES C CXX)

# Library sources (exclude xdiff-cli.c and xdiff-moved.c)
file(GLOB SRC "*.c" "*.h")
list(SORT SRC)
list(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/xdiff-cli.c")
list(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/xdiff-moved.c")

add_library(libxdiff STATIC ${SRC})

# CLI executable
add_executable(xdiff xdiff-cli.c xdiff-moved.c)
target_link_libraries(xdiff libxdiff)

# Installation
install(TARGETS xdiff DESTINATION bin)
#install(TARGETS libxdiff ARCHIVE DESTINATION lib)

# Testing setup
enable_testing()

# Add tests subdirectory
add_subdirectory(tests)
