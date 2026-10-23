# Compile-only check that core/, port/esp32/ and the public API build under ESP-IDF.
# CI builds it for several IDF versions and targets; it is not flashed.
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sota_port_smoke)
