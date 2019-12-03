cmake_minimum_required(VERSION 3.10)

project(udmalib VERSION 0.1.0 DESCRIPTION "UDMA buffers manager")

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wpedantic")

set(SOURCES
    "udma.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "udma.h")

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_SOURCE_DIR}/install/lib
        PUBLIC_HEADER DESTINATION ${CMAKE_SOURCE_DIR}/install/include/udmalib)

