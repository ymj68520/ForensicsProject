# LLM集成模块CMake配置

cmake_minimum_required(VERSION 3.16)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CURL_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})

# LLM集成库
add_library(llm_integration STATIC
    LLMIntegrationDataTypes.h
    TaskClassifier.h
    TaskClassifier.cpp
    LMStudioClient.h
    LMStudioClient.cpp
    ModelManager.h
    ModelManager.cpp
    AIAnalysisService.h
    AIAnalysisService.cpp
    LLMHttpServer.h
    LLMHttpServer.cpp
)

# 链接库
target_link_libraries(llm_integration
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    nlohmann_json::nlohmann_json
    Threads::Threads
)

# 添加编译选项
target_compile_options(llm_integration PRIVATE
    -Wall
    -Wextra
    -O2
)

# 示例程序
add_executable(llm_example example_usage.cpp)
target_link_libraries(llm_example
    llm_integration
)

# 设置包含目录
target_include_directories(llm_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 安装规则
install(TARGETS llm_integration
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    LLMIntegrationDataTypes.h
    TaskClassifier.h
    LMStudioClient.h
    ModelManager.h
    AIAnalysisService.h
    LLMHttpServer.h
    DESTINATION include/LLMIntegration
)

install(FILES llm_config.json
    DESTINATION config
)

# 测试
enable_testing()

# 单元测试
add_executable(test_task_classifier
    tests/test_task_classifier.cpp
)
target_link_libraries(test_task_classifier
    llm_integration
)
add_test(NAME TaskClassifierTest COMMAND test_task_classifier)

add_executable(test_model_manager
    tests/test_model_manager.cpp
)
target_link_libraries(test_model_manager
    llm_integration
)
add_test(NAME ModelManagerTest COMMAND test_model_manager)

add_executable(test_ai_service
    tests/test_ai_service.cpp
)
target_link_libraries(test_ai_service
    llm_integration
)
add_test(NAME AIServiceTest COMMAND test_ai_service)

# 文档生成
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    set(DOXYGEN_INPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
    
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
                 ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    
    add_custom_target(docs
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "生成API文档" VERBATIM
    )
endif()

# 打包配置
set(CPACK_PACKAGE_NAME "LLMIntegration")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "数字取证工具大模型集成模块")
set(CPACK_PACKAGE_VENDOR "ForensicsProject")
set(CPACK_GENERATOR "TGZ;DEB")

include(CPack)