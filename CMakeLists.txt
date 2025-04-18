cmake_minimum_required(VERSION 3.13)

project(display_drivers LANGUAGES C CXX)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# For lsp
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(NAME st7735_driver)

add_subdirectory(src)
add_subdirectory(tests)
