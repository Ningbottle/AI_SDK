# 1. 确定最低 CMake 版本
cmake_minimum_required(VERSION 3.10)

# 2. 设置项目名称
project(AI_Chat_SDK LANGUAGES CXX)

# 3. 设置 SDK 名称
set(SDK_NAME "AI_Chat_SDK")

# 4. 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 5. 收集源文件和头文件
file(GLOB SDK_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB SDK_INCLUDE
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

# 6. 生成静态库
add_library(${SDK_NAME} STATIC
    ${SDK_SRC}
    ${SDK_INCLUDE}
)

# 7. 设置静态库输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/build/lib
)

# 8. 设置头文件目录
target_include_directories(${SDK_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 9. 找 OpenSSL
find_package(OpenSSL REQUIRED)

# 10. 设置库搜索目录
link_directories(/usr/local/lib)

# 11. 定义 CPPHTTPLIB_OPENSSL_SUPPORT 宏
target_compile_definitions(${SDK_NAME}
    PUBLIC
        CPPHTTPLIB_OPENSSL_SUPPORT
)

# 12. 链接第三方库
target_link_libraries(${SDK_NAME}
    PUBLIC
        jsoncpp
        fmt
        spdlog
        sqlite3
        OpenSSL::SSL
        OpenSSL::Crypto
)

# 13. 安装静态库到 /usr/local/lib
install(TARGETS ${SDK_NAME}
    ARCHIVE DESTINATION lib
)

# 14. 安装头文件到 /usr/local/include/ai_chat_sdk
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include/ai_chat_sdk
    FILES_MATCHING PATTERN "*.h"
)
