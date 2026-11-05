cmake_minimum_required(VERSION 3.23)
project(dspsigselector LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(${PROJECT_NAME} SHARED "src/main.c")
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
