# CMakeLists.txt for Forensics Project Tests
cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(GTest QUIET)
find_package(GMock QUIET)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

# Include directories
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CURL_INCLUDE_DIRS})

# Enhanced SQLite Helper Tests
add_executable(test_sqlite_helper_enhanced test_sqlite_helper_enhanced.cpp)

target_link_libraries(
  test_sqlite_helper_enhanced
  ${GTEST_LIBRARIES}
  ${GMOCK_LIBRARIES}
  ${CURL_LIBRARIES}
  sqlite3
  pthread
  nlohmann_json::nlohmann_json)

# Add test to CTest
add_test(NAME SQLiteHelperEnhancedTests COMMAND test_sqlite_helper_enhanced)

# HTTP Server Integration Tests
add_executable(test_http_server_integration test_http_server_integration.cpp)

target_link_libraries(
  test_http_server_integration
  ${GTEST_LIBRARIES}
  ${GMOCK_LIBRARIES}
  ${CURL_LIBRARIES}
  sqlite3
  pthread
  nlohmann_json::nlohmann_json
  asio
  crow
  boost_system
  boost_thread)

# Add test to CTest
add_test(NAME HTTPServerIntegrationTests COMMAND test_http_server_integration)

# Performance Tests
add_executable(test_performance test_performance.cpp)

target_link_libraries(
  test_performance
  ${GTEST_LIBRARIES}
  ${GMOCK_LIBRARIES}
  ${CURL_LIBRARIES}
  sqlite3
  pthread
  nlohmann_json::nlohmann_json)

# Add test to CTest
add_test(NAME PerformanceTests COMMAND test_performance)

# Test scripts for running different test suites
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/run_unit_tests.sh.in"
               "${CMAKE_CURRENT_BINARY_DIR}/run_unit_tests.sh" @ONLY)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/run_integration_tests.sh.in"
               "${CMAKE_CURRENT_BINARY_DIR}/run_integration_tests.sh" @ONLY)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/run_performance_tests.sh.in"
               "${CMAKE_CURRENT_BINARY_DIR}/run_performance_tests.sh" @ONLY)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/run_all_tests.sh.in"
               "${CMAKE_CURRENT_BINARY_DIR}/run_all_tests.sh" @ONLY)

# Make scripts executable
file(
  CHMOD
  ${CMAKE_CURRENT_BINARY_DIR}/run_unit_tests.sh
  FILE_PERMISSIONS
  OWNER_READ
  OWNER_WRITE
  OWNER_EXECUTE
  GROUP_READ
  GROUP_EXECUTE
  WORLD_READ
  WORLD_EXECUTE)
file(
  CHMOD
  ${CMAKE_CURRENT_BINARY_DIR}/run_integration_tests.sh
  FILE_PERMISSIONS
  OWNER_READ
  OWNER_WRITE
  OWNER_EXECUTE
  GROUP_READ
  GROUP_EXECUTE
  WORLD_READ
  WORLD_EXECUTE)
file(
  CHMOD
  ${CMAKE_CURRENT_BINARY_DIR}/run_performance_tests.sh
  FILE_PERMISSIONS
  OWNER_READ
  OWNER_WRITE
  OWNER_EXECUTE
  GROUP_READ
  GROUP_EXECUTE
  WORLD_READ
  WORLD_EXECUTE)
file(
  CHMOD
  ${CMAKE_CURRENT_BINARY_DIR}/run_all_tests.sh
  FILE_PERMISSIONS
  OWNER_READ
  OWNER_WRITE
  OWNER_EXECUTE
  GROUP_READ
  GROUP_EXECUTE
  WORLD_READ
  WORLD_EXECUTE)

# Windows Files Analyzer Tests
add_executable(
  test_windows_analyzer
  test_windows_analyzer.cpp
  ../WindowsFilesAnalyzer/WindowsFilesAnalyzerCore.cpp
  ../WindowsFilesAnalyzer/WindowsAnalysisDatabase.cpp
  ../WindowsFilesAnalyzer/WindowsRegistryParser.cpp
  ../WindowsFilesAnalyzer/WindowsEventLogParser.cpp
  ../WindowsFilesAnalyzer/WindowsArtifactsParsers.cpp
  ../AuditLog/AuditLog.cpp
  ../DatabaseManager/DatabaseManager.cpp
  ../DatabaseManager/FileExtractor/FileExtractor.cpp)

target_link_libraries(test_windows_analyzer sqlite3 pthread tsk)

# Add test to CTest
add_test(NAME WindowsFilesAnalyzerTests COMMAND test_windows_analyzer)

# Installation rules for test executables (optional)
install(TARGETS test_sqlite_helper_enhanced test_http_server_integration
                test_performance test_windows_analyzer
        RUNTIME DESTINATION bin/tests)

# Installation rules for test scripts
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/run_unit_tests.sh
        ${CMAKE_CURRENT_BINARY_DIR}/run_integration_tests.sh
        ${CMAKE_CURRENT_BINARY_DIR}/run_performance_tests.sh
        ${CMAKE_CURRENT_BINARY_DIR}/run_all_tests.sh
  DESTINATION bin/tests
  PERMISSIONS
    OWNER_READ
    OWNER_WRITE
    OWNER_EXECUTE
    GROUP_READ
    GROUP_EXECUTE
    WORLD_READ
    WORLD_EXECUTE)
