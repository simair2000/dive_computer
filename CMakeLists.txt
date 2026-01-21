cmake_minimum_required(VERSION 3.10)
project(ScubaProject C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# MSVC에서 -g 경고 제거
if(NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
endif()

add_executable(main main.c buhlmann.c)

# 실행 타겟
add_custom_target(run
    COMMAND main
    DEPENDS main
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Debug
)

add_compile_options(/utf-8)