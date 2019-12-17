cmake_minimum_required(VERSION 3.10)

project(cynq VERSION 0.1.0 DESCRIPTION "C++ FPGA Manager")

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wpedantic")

set(SOURCES
    "cynq.cpp"
    "mmio.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

add_dependencies(${PROJECT_NAME} bit_patch)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} libstdc++fs.a)

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "cynq.h;mmio.h;fpga.h")

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_SOURCE_DIR}/install/lib
        PUBLIC_HEADER DESTINATION ${CMAKE_SOURCE_DIR}/install/include/cynq)

