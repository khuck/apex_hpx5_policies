# Copyright (c) 2015 University of Oregon
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)
cmake_policy(SET CMP0042 NEW) 

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules")

project(apex_coalesce_policy CXX)

include(GitExternal)

# add_definitions(-std=c++11)
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++14" COMPILER_SUPPORTS_CXX14)
if(COMPILER_SUPPORTS_CXX14)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
    message(INFO " Compiler ${CMAKE_CXX_COMPILER} has C++14 support.")
else()
    CHECK_CXX_COMPILER_FLAG("-std=c++1y" COMPILER_SUPPORTS_CXX1Y)
    if(COMPILER_SUPPORTS_CXX1Y)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
        message(INFO " Compiler ${CMAKE_CXX_COMPILER} has early C++14 support.")
    else()
        message(FATAL_ERROR " Compiler ${CMAKE_CXX_COMPILER} does not have any C++14 support; please use a newer compiler.")
    endif()
endif()

find_package(PkgConfig REQUIRED)
pkg_search_module(APEX REQUIRED apex)
message(INFO " Found APEX: ${APEX_LIBRARIES}")
include_directories(${APEX_INCLUDE_DIRS})
link_directories(${APEX_LIBRARY_DIRS})
set(LIBS ${LIBS} ${APEX_LIBRARIES})

git_external(rapidjson
    https://github.com/miloyip/rapidjson.git
    master
    VERBOSE)

find_path(
    RAPIDJSON_INCLUDE_DIR
    NAMES rapidjson
    PATHS ${PROJECT_SOURCE_DIR}/rapidjson/include)

if(RAPIDJSON_INCLUDE_DIR)
    message(INFO " Found rapidjson at ${RAPIDJSON_INCLUDE_DIR}")
    include_directories(${RAPIDJSON_INCLUDE_DIR})
else()
    message(FATAL_ERROR " rapidjson not found. This should have been checked out automatically. "
        "Try manually check out https://github.com/miloyip/rapidjson.git to ${PROJECT_SOURCE_DIR}")
endif()

add_subdirectory(src)

