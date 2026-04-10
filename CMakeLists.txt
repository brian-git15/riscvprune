cmake_minimum_required(VERSION 3.20)
project(NounwindLTO)

find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_llvm_pass_plugin(NounwindLTO
  lib/Transform/NounwindPass.cpp
  lib/Analysis/NounwindAnalysis.cpp
  lib/Plugin.cpp
)