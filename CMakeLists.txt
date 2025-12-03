cmake_minimum_required(VERSION 3.20)
project(ForensicAnalyzer VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(SQLite3 REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)

# TSK library paths
if(WIN32)
    # Windows: Use local TSK build
    set(TSK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/sleuthkit" CACHE PATH "TSK include directory")
    set(TSK_LIBRARY_DIR "${CMAKE_SOURCE_DIR}/sleuthkit/win32/x64/Release_NoLibs" CACHE PATH "TSK library directory")
    set(TSK_LIBRARIES "libtsk.lib")
    set(PLATFORM_LIBRARIES "ole32")
else()
    # Linux/Unix: Use system-installed TSK
    set(TSK_INCLUDE_DIR "/usr/local/include" CACHE PATH "TSK include directory")
    set(TSK_LIBRARY_DIR "/usr/local/lib" CACHE PATH "TSK library directory")
    set(TSK_LIBRARIES "tsk")
    set(PLATFORM_LIBRARIES "pthread" "dl")
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${TSK_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    # Add Crow include path if needed, e.g., ${CMAKE_SOURCE_DIR}/crow/include
)

link_directories(${TSK_LIBRARY_DIR})

# Source files
set(SOURCES
    main.cpp
    ImageAnalyzer/ImageAnalyzer.cpp
    DatabaseManager/DatabaseManager.cpp
    DatabaseManager/EventExtractor/EventExtractor.cpp
    DatabaseManager/FileClassifier/FileClassifier.cpp
    DatabaseManager/FileExtractor/FileExtractor.cpp
    ImageAnalyzer/XFSHelper.cpp
    ImageAnalyzer/NativeFilesystemWalker.cpp
    HTTPServer/HTTPserver.cpp
)

# Create executable
add_executable(forensic_analyzer ${SOURCES})

# Link libraries
if(WIN32)
    target_link_libraries(forensic_analyzer
        ${TSK_LIBRARIES}
        sqlite3.lib
        ${Boost_LIBRARIES}
        ${PLATFORM_LIBRARIES}
    )
else()
    target_link_libraries(forensic_analyzer
        ${TSK_LIBRARIES}
        ewf
        ${SQLite3_LIBRARIES}
        ${Boost_LIBRARIES}
        ${PLATFORM_LIBRARIES}
    )
endif()

# Installation
install(TARGETS forensic_analyzer DESTINATION bin)

