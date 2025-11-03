cmake_minimum_required(VERSION 3.10)

project(xdiff VERSION "0.1" LANGUAGES C)

# Library sources (exclude xdiff-cli.c)
file(GLOB SRC "*.c" "*.h")
list(SORT SRC)
list(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/xdiff-cli.c")

add_library(xdiff STATIC ${SRC})

# CLI executable
add_executable(xdiff-cli xdiff-cli.c)
target_link_libraries(xdiff-cli xdiff)

# Installation
install(TARGETS xdiff-cli DESTINATION bin RENAME xdiff)
install(TARGETS xdiff ARCHIVE DESTINATION lib)
