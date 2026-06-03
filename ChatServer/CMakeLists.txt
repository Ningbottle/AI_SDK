cmake_minimum_required(VERSION 3.14)
project(AIChatServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

# ============================================================
# 1. 自动下载 gflags（系统未安装时使用）
# ============================================================
include(FetchContent)
FetchContent_Declare(
    gflags
    GIT_REPOSITORY https://github.com/gflags/gflags.git
    GIT_TAG        v2.2.2
)
set(gflags_BUILD_STATIC_LIBS ON)
set(gflags_BUILD_SHARED_LIBS OFF)
set(gflags_INSTALL OFF)
FetchContent_MakeAvailable(gflags)

# ============================================================
# 2. 收集源文件
# ============================================================
set(SOURCES
    main.cpp
    ChatServer.cpp
    ../SDK/src/ChatSDK.cpp
    ../SDK/src/DeepSeekProvider.cpp
    ../SDK/src/GLMProvider.cpp
    ../SDK/src/KimiProvider.cpp
    ../SDK/src/OllamalProvider.cpp
    ../SDK/src/LLMManager.cpp
    ../SDK/src/SessionManager.cpp
    ../SDK/src/DataManager.cpp
    ../SDK/src/util/myLog.cpp
)

# ============================================================
# 3. 创建可执行文件
# ============================================================
add_executable(AIChatServer ${SOURCES})

# 4. 头文件搜索路径
target_include_directories(AIChatServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../SDK/include
    ${gflags_BINARY_DIR}/include
)

# 5. 查找系统依赖
find_package(OpenSSL REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# 6. 编译宏
target_compile_definitions(AIChatServer PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
)

# 7. JSONCPP 头文件
target_include_directories(AIChatServer PRIVATE
    ${JSONCPP_INCLUDE_DIRS}
)

# 8. 链接库
target_link_libraries(AIChatServer PRIVATE
    fmt::fmt
    spdlog::spdlog
    OpenSSL::SSL
    OpenSSL::Crypto
    cpp-httplib
    ${JSONCPP_LIBRARIES}
    SQLite::SQLite3
    gflags::gflags
)

# 9. 安装
install(TARGETS AIChatServer RUNTIME DESTINATION bin)
