cmake_minimum_required(VERSION 3.10)

# set the project name
project(IFCC CXX)
set(CMAKE_CXX_STANDARD 17)

option(USE_MMAP "Adds a command line options to parse IFC files from memory mapped files using Boost.Iostreams" OFF)

# find_package(RocksDB REQUIRED)

# IFCOpenShell
if("${IFC_INCLUDE_DIR}" STREQUAL "")
   
    if(UNIX)
        find_path(IFC_INCLUDE_DIR ifcgeom/IfcGeom.h
        PATHS
        /usr/local/include/ifcgeom/
        )
    endif()

    if(NOT IFC_INCLUDE_DIR)
     set(IFC_INCLUDE_DIR "" CACHE PATH "Path to IFCOpenShell include directory")
        message(FATAL_ERROR "Unable to find IFCOpenShell include directory. Specify IFC_INCLUDE_DIR manually")
    endif()
endif()

if(IFC_INCLUDE_DIR)
    set(IFC_INCLUDE_DIR ${IFC_INCLUDE_DIR} CACHE FILEPATH "IFCOpenShell header files")
    message(STATUS "Looking for IFCOpenShell include files in: ${IFC_INCLUDE_DIR}")

    if(UNIX)
        set(IFCGEOM_INCLUDE_DIR "${IFC_INCLUDE_DIR}/ifcgeom")
        set(IFCGEOMSCHEMA_INCLUDE_DIR "${IFC_INCLUDE_DIR}/ifcgeom_schema_agnostic")
        set(IFCPARSE_INCLUDE_DIR "${IFC_INCLUDE_DIR}/ifcparse")
        message(STATUS "Looking for ifcgeom include files in: ${IFCGEOM_INCLUDE_DIR}")
        message(STATUS "Looking for ifcgeom_schema_agnostic include files in: ${IFCGEOMSCHEMA_INCLUDE_DIR}")
        message(STATUS "Looking for ifcparse include files in: ${IFCPARSE_INCLUDE_DIR}")
    endif(UNIX)
endif()

set(IFC_LIBRARY_NAMES 
Serializers    
Serializers_ifc2x3 Serializers_ifc4 Serializers_ifc4x1 Serializers_ifc4x2 Serializers_ifc4x3 Serializers_ifc4x3_add1 Serializers_ifc4x3_add2
Serializers_ifc4x3_rc1 Serializers_ifc4x3_rc2 Serializers_ifc4x3_rc3 Serializers_ifc4x3_rc4 Serializers_ifc4x3_tc1 
IfcGeom
IfcGeom_ifc2x3 IfcGeom_ifc4 IfcGeom_ifc4x1 IfcGeom_ifc4x2 IfcGeom_ifc4x3 IfcGeom_ifc4x3_add1 IfcGeom_ifc4x3_add2 
IfcGeom_ifc4x3_rc1 IfcGeom_ifc4x3_rc2 IfcGeom_ifc4x3_rc3 IfcGeom_ifc4x3_rc4 IfcGeom_ifc4x3_tc1
Serializers IfcParse 
)

if("${IFC_LIBRARY_DIR}" STREQUAL "")
    if(UNIX)
        find_path(IFC_LIBRARY_DIR libIfcGeom.a
        PATHS
        ${IFC_INCLUDE_DIR}/../lib/
    )
    elseif(WIN32)
        find_path(IFC_LIBRARY_DIR IfcGeom.lib
        PATHS
        ${IFC_INCLUDE_DIR}/../lib/
    )
    endif()

    if(IFC_LIBRARY_DIR)
        message(STATUS "Found IFCOpenShell lib files in: ${IFC_LIBRARY_DIR}")
    else()
        message(FATAL_ERROR "Unable to find IFCOpenShell lib directory. Specify IFC_LIBRARY_DIR manually")    
    endif()
else()
    set(IFC_LIBRARY_DIR ${IFC_LIBRARY_DIR} CACHE FILEPATH "IFCOpenShell header files")
    message(STATUS "Looking for IFCOpenShell lib files in: ${IFC_LIBRARY_DIR}")
endif()

foreach(LIB ${IFC_INCLUDE_DIR})
    message(STATUS ${IFC_LIBRARY_DIR})
endforeach()

if(WIN32)
    file(GLOB IFC_LIBRARY_FILES "${IFC_LIBRARY_DIR}/*.lib")
elseif(UNIX)
    foreach(file ${IFC_LIBRARY_NAMES})
        list(APPEND IFC_LIBRARY_FILES "${IFC_LIBRARY_DIR}/lib${file}.a")
    endforeach()
endif()

# Boost
if(WIN32 AND("$ENV{CONDA_BUILD}" STREQUAL ""))
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_RUNTIME OFF)
    set(Boost_USE_MULTITHREADED ON)

    # Disable Boost's autolinking as the libraries to be linked to are supplied
    # already by CMake, and wrong libraries would be asked for when code is
    # compiled with a toolset different from default.
    if(MSVC)
        add_definitions(-DBOOST_ALL_NO_LIB)

        # Necessary for boost version >= 1.67
        set(BCRYPT_LIBRARIES "bcrypt.lib")
    endif()
else()
    # Disable Boost's autolinking as the libraries to be linked to are supplied
    # already by CMake, and it's going to conflict if there are multiple, as is
    # the case in conda-forge's libboost feedstock.
    add_definitions(-DBOOST_ALL_NO_LIB)

    if(WIN32)
        # Necessary for boost version >= 1.67
        set(BCRYPT_LIBRARIES "bcrypt.lib")
    endif()
endif()

set(BOOST_COMPONENTS ${BOOST_COMPONENTS} filesystem)
if(USE_MMAP)
    set(BOOST_COMPONENTS ${BOOST_COMPONENTS} iostreams)
    add_definitions(-DUSE_MMAP)
endif()

find_package(Boost REQUIRED COMPONENTS ${BOOST_COMPONENTS})

message(STATUS "Boost include files found in ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost libraries found in ${Boost_LIBRARIES}")

if(UNIX)
    include_directories("/usr/local/include" ${Boost_INCLUDE_DIRS}${IFC_INCLUDE_DIR} ${IFCGEOM_INCLUDE_DIR} ${IFCGEOMSCHEMA_INCLUDE_DIR} ${IFCPARSE_INCLUDE_DIR}  )
elseif(WIN32)
    include_directories("/usr/local/include" ${Boost_INCLUDE_DIRS} ${IFC_INCLUDE_DIR} )
endif()

add_executable(${PROJECT_NAME} 
IFCC.cpp
)

link_directories("/usr/local/lib" ${Boost_LIBRARY_DIRS}  ${IFC_LIBRARY_DIR})

if(UNIX)
    # target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} ${BCRYPT_LIBRARIES}  RocksDB::rocksdb)
    target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} ${BCRYPT_LIBRARIES} )
elseif(WIN32)
    # target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} ${BCRYPT_LIBRARIES}  RocksDB::rocksdb ws2_32.lib)
    target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} ${BCRYPT_LIBRARIES} ws2_32.lib)
endif()

message(STATUS "Boost libraries are:")
foreach(LIB ${Boost_LIBRARIES})
    message(STATUS "    ${LIB}")
endforeach()

message(STATUS "IFC libraries are:")
foreach(LIB ${IFC_LIBRARY_FILES})
    message(STATUS "    ${LIB}")
    target_link_libraries(${PROJECT_NAME} ${LIB})
endforeach()

get_target_property(LINK_LIBS ${PROJECT_NAME} LINK_LIBRARIES)
message(STATUS "Linked libraries for ${PROJECT_NAME}: ${LINK_LIBS}")