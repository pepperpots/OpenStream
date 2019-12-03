cmake_minimum_required(VERSION 3.10)

project(bit_patch VERSION 0.1.0 DESCRIPTION "Transform bitstream of one PR region to another")

set(CMAKE_C_FLAGS_DEBUG "-g -Wall -Wpedantic")
set(CMAKE_C_FLAGS_RELEASE "-O3 -Wall -Wpedantic")

set(SOURCES
    "bit_patch.c"
    "bit_patch_driver.c"
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "bit_patch.h")

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_SOURCE_DIR}/install/lib
        PUBLIC_HEADER DESTINATION ${CMAKE_SOURCE_DIR}/install/include/bit_patch)

