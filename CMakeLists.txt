cmake_minimum_required(VERSION 3.10)

# set the project name
project(IFCC CXX)
set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} 
IFCC.cpp
)