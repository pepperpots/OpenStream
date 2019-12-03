cmake_minimum_required(VERSION 3.10)

project(fos VERSION 0.1.0 DESCRIPTION "FPGA Operating System")

add_subdirectory(bit_patch)
add_subdirectory(cynq)
add_subdirectory(udmalib)

