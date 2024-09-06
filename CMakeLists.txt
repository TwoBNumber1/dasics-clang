cmake_minimum_required(VERSION 3.20)
project(macro-guard)

# set(CMAKE_CXX_STANDARD 14)

# find_package(LLVM REQUIRED CONFIG)

# message(STATUS "Using LLVM ${LLVM_PACKAGE_VERSION}")

# add_definitions(${LLVM_DEFINITIONS})
# include_directories(${LLVM_INCLUDE_DIRS})
# link_directories(${LLVM_LIBRARY_DIR})

# list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
# include(AddLLVM)

# find_package(Clang REQUIRED CONFIG)
# include_directories(${CLANG_INCLUDE_DIRS})
# #list(APPEND CMAKE_MODULE_PATH "${CLANG_CMAKE_DIR}")

# # Handle RTTI stuff, which often leads to error
# if(NOT ${LLVM_ENABLE_RTTI})
#   if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR
#       "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
#       "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
#     message(STATUS "Disable RTTI")
#   elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
#     message(STATUS "Disable RTTI")
#   endif()
#   # Do not give any flags for other less widely used
#   # compilers
# endif()


#===============================================================================
# 1. LOAD Clang CONFIGURATION
#===============================================================================
# Set this to a valid LLVM installation dir
set(CT_Clang_INSTALL_DIR "" CACHE PATH "Clang installation directory")

# Add the location of ClangConfig.cmake to CMake search paths (so that
# find_package can locate it)
list(APPEND CMAKE_PREFIX_PATH "${CT_Clang_INSTALL_DIR}/lib/cmake/clang/")

find_package(Clang REQUIRED CONFIG)
if("${LLVM_VERSION_MAJOR}" VERSION_LESS 18)
  message(FATAL_ERROR "Found LLVM ${LLVM_VERSION_MAJOR}, but need LLVM 18 or above")
endif()

# HelloWorld includes headers from LLVM and Clang - update the include paths
# accordingly
include_directories(SYSTEM "${LLVM_INCLUDE_DIRS};${CLANG_INCLUDE_DIRS}")

#===============================================================================
# 2. CLANG-TUTOR BUILD CONFIGURATION
#===============================================================================
# Use the same C++ standard as LLVM does
set(CMAKE_CXX_STANDARD 17 CACHE STRING "")

# LLVM is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# -fvisibility-inlines-hidden is set when building LLVM and on Darwin warnings
# are triggered if llvm-tutor is built without this flag (though otherwise it
# builds fine). For consistency, add it here too.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fvisibility-inlines-hidden"
  SUPPORTS_FVISIBILITY_INLINES_HIDDEN_FLAG)
if(${SUPPORTS_FVISIBILITY_INLINES_HIDDEN_FLAG} EQUAL "1")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
endif()


set(_SOURCE_FILES
    MacroGuardPragma.cpp
    MacroGuardValidator.cpp
    
    )

add_library(MacroGuard MODULE
    ${_SOURCE_FILES})
# Allow undefined symbols in shared objects on Darwin (this is the default
# behaviour on Linux)
target_link_libraries(MacroGuard
"$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")

set(_CodeRefactor_SOURCE
  CodeRefactor_recompile.cpp
)
add_library(CodeRefactor MODULE
                 ${_CodeRefactor_SOURCE})
# Allow undefined symbols in shared objects on Darwin (this is the default
# behaviour on Linux)
target_link_libraries(CodeRefactor
"$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")

