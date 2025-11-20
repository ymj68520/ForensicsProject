cmake_minimum_required(VERSION 3.20)
project(ForensicAnalyzer VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(SQLite3 REQUIRED)

# TSK library paths (adjust based on your installation)
set(TSK_INCLUDE_DIR "/usr/local/include" CACHE PATH "TSK include directory")
set(TSK_LIBRARY_DIR "/usr/local/lib" CACHE PATH "TSK library directory")

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${TSK_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

link_directories(${TSK_LIBRARY_DIR})

# Source files
set(SOURCES
    src/main.cpp
    src/ImageAnalyzer.cpp
    src/DatabaseManager.cpp
    src/EventExtractor.cpp
    src/FileClassifier.cpp
)

# Create executable
add_executable(forensic_analyzer ${SOURCES})

# Link libraries
target_link_libraries(forensic_analyzer
    tsk
    ewf
    ${SQLite3_LIBRARIES}
    pthread
    dl
)

# Installation
install(TARGETS forensic_analyzer DESTINATION bin)
