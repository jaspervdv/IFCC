cmake_minimum_required(VERSION 3.10)
include(FetchContent)

# set the project name
project(IFCC CXX)
set(CMAKE_CXX_STANDARD 17)

# set
option(LINK_IFCSWAP "Link IfcSwap executable to support IFCC Fragments output" OFF)

# Fetch minizip
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
set(MZ_ZLIB ON CACHE BOOL "" FORCE)
set(MZ_FETCH_LIBS ON CACHE BOOL "" FORCE)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(MZ_BZIP2 OFF CACHE BOOL "" FORCE)
set(MZ_LZMA OFF CACHE BOOL "" FORCE)
set(MZ_PPMD OFF CACHE BOOL "" FORCE)
set(MZ_ZSTD OFF CACHE BOOL "" FORCE)
set(MZ_PKCRYPT OFF CACHE BOOL "" FORCE)
set(MZ_WZAES OFF CACHE BOOL "" FORCE)
set(MZ_OPENSSL OFF CACHE BOOL "" FORCE)
set(MZ_ICONV OFF CACHE BOOL "" FORCE)
set(MZ_FORCE_FETCH_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    minizip
    GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng.git
    GIT_TAG master
)
FetchContent_MakeAvailable(minizip)

# Executable
add_executable(${PROJECT_NAME} 
src/IFCC.cpp
src/IfcClass.cpp
src/IfcFile.cpp
include/IfcClass.hpp
include/IfcFile.hpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        src
    PUBLIC
        include
)

# Link
target_include_directories(${PROJECT_NAME} PRIVATE )
target_link_libraries(${PROJECT_NAME} PRIVATE minizip)

if(LINK_IFCSWAP)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/supportApps/IfcSwap/dist/IfcSwap.exe")
        message(ERROR "IfcSwap.exe not found. Please run BuildIfcSwap.bat manually.")
    endif()

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/supportApps/IfcSwap/dist/IfcSwap.exe"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/supportApps/IfcSwap/dist/web-ifc-node.wasm"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying IfcSwap.exe to output directory"
    )
else()
    message(STATUS "IfcSwap build disabled")
endif()