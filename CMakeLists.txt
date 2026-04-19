cmake_minimum_required(VERSION 3.20)
project(NounwindLTO)

# Ensure we use C++17 or higher (LLVM 18+ requires it)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Require LLVM 18.x (avoid `find_package(LLVM 18)` — some CMake/LLVM pairs reject 18.1.x)
find_package(LLVM REQUIRED CONFIG)
if(LLVM_PACKAGE_VERSION VERSION_LESS "18.0" OR LLVM_PACKAGE_VERSION VERSION_GREATER_EQUAL "19.0")
  message(FATAL_ERROR "Need LLVM 18.x, found ${LLVM_PACKAGE_VERSION}")
endif()

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake from: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_definitions(${LLVM_DEFINITIONS})

add_llvm_pass_plugin(NounwindLTO
  lib/Analysis/NounwindAnalysis.cpp
  lib/Transform/NounwindPass.cpp
  lib/Plugin.cpp
)