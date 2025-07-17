cmake_minimum_required(VERSION 3.10)

project(ArduinoEEPROMProgrammer LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Architecture
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")

# Gather sources (C and C++)
file(GLOB_RECURSE SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/*.c"
  "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

# Show what's found
message(STATUS "Source files: ${SOURCES}")

# Include path (must be full or relative to CMAKE_SOURCE_DIR)
# include_directories(${CMAKE_SOURCE_DIR}/include)

# Define executable
add_executable(arduino_romprog ${SOURCES})
