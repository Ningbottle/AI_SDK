# 1. 指定 CMake 最低版本
# cmake_minimum_required means the minimum CMake version required by this project.
# cmake_minimum_required（最低 CMake 版本）用于保证当前 CMake 语法被支持。
cmake_minimum_required(VERSION 3.10)

# 2. 定义项目名称
# project defines the project name and enables default languages like C/C++.
# project（项目）会创建项目名，并启用默认语言环境。
project(LLMTest)

# 3. 设置 C++ 标准为 C++17
# CMAKE_CXX_STANDARD specifies the C++ language standard.
# CMAKE_CXX_STANDARD（C++ 标准）指定使用哪个版本的 C++。
set(CMAKE_CXX_STANDARD 17)
# 要求必须使用 C++17，如果编译器不支持就报错
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 4. 设置构建类型为 Debug
# Debug build keeps debug symbols and disables most optimizations.
# Debug（调试构建）会保留调试信息，方便 gdb 调试。
set(CMAKE_BUILD_TYPE Debug)

# 5. 设置可执行文件输出目录
# CMAKE_RUNTIME_OUTPUT_DIRECTORY controls where executable files are generated.
# CMAKE_RUNTIME_OUTPUT_DIRECTORY（运行文件输出目录）控制生成的可执行文件放在哪里。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 6. 收集需要参与编译的源文件
# sources is just a user-defined variable.
# sources（源文件列表）是你自己定义的变量名。
set(sources
    TestLLM.cpp
    ../src/DeepSeekProvider.cpp
    ../src/util/myLog.cpp
    ../src/GLMProvider.cpp
    ../src/KimiProvider.cpp
    ../src/OllamalProvider.cpp
)

# 7. 创建可执行程序 target
# add_executable creates a build target.
# add_executable（创建可执行目标）会生成名为 LLMTest 的可执行文件。
add_executable(LLMTest ${sources})

# 8. 给 LLMTest 添加头文件搜索路径
# target_include_directories adds include paths for a specific target.
# target_include_directories（目标头文件路径）只影响 LLMTest 这个目标。
target_include_directories(LLMTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 9. 查找第三方库
# find_package locates installed libraries and creates imported targets.
# find_package（查找包）用于寻找系统中已经安装好的第三方库。

# OpenSSL is required by cpp-httplib for HTTPS support.
# OpenSSL 用于支持 HTTPS 请求。
find_package(OpenSSL REQUIRED)

# spdlog is used for logging.
# spdlog 用于日志输出。
find_package(spdlog REQUIRED)

# fmt is used for string formatting.
# fmt 用于格式化字符串，spdlog 底层常用它。
find_package(fmt REQUIRED)

# GTest is used for unit testing.
# GTest 用于单元测试。
find_package(GTest REQUIRED)

# PkgConfig helps CMake find libraries that provide .pc files.
# PkgConfig 用于查找通过 pkg-config 管理的库，比如 jsoncpp。
find_package(PkgConfig REQUIRED)

# 10. 查找 jsoncpp
# pkg_check_modules reads jsoncpp's pkg-config information.
# pkg_check_modules（通过 pkg-config 查找模块）会得到 jsoncpp 的 include 路径和链接库。
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# 11. 添加 jsoncpp 的头文件路径
# JSONCPP_INCLUDE_DIRS comes from pkg_check_modules.
# JSONCPP_INCLUDE_DIRS 是 pkg_check_modules 找到的 jsoncpp 头文件路径。
target_include_directories(LLMTest PRIVATE
    ${JSONCPP_INCLUDE_DIRS}
)

# 12. 定义 cpp-httplib 的 HTTPS 开关
# target_compile_definitions adds compiler macros for a specific target.
# target_compile_definitions（目标编译宏）等价于给编译器加 -DCPPHTTPLIB_OPENSSL_SUPPORT。
target_compile_definitions(LLMTest PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
)

# 13. 链接第三方库
# target_link_libraries links libraries to the executable target.
# target_link_libraries（目标链接库）告诉链接器 LLMTest 需要哪些库。
target_link_libraries(LLMTest PRIVATE
    fmt::fmt              # fmt imported target，格式化库
    spdlog::spdlog        # spdlog imported target，日志库
    GTest::gtest          # GTest imported target，测试库

    OpenSSL::SSL          # OpenSSL SSL library，HTTPS/SSL 支持
    OpenSSL::Crypto       # OpenSSL Crypto library，加密算法支持

    cpp-httplib
    ${JSONCPP_LIBRARIES}  # jsoncpp libraries，由 pkg-config 找到
)
