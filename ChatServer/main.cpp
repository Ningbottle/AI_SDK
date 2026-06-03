#include <gflags/gflags.h>
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <memory>
#include <thread>
#include <vector>
#include <spdlog/common.h>

#include "ChatServer.h"
#include "../SDK/include/util/myLog.h"

// ============================================================
// 1. 命令行参数定义
// ============================================================

// --- 服务器基础参数 ---
DEFINE_string(host, "0.0.0.0",
    "服务器绑定地址");
DEFINE_int32(port, 8080,
    "服务器监听端口");

// --- 日志参数 ---
DEFINE_string(log_level, "INFO",
    "日志级别，可选: TRACE / DEBUG / INFO / WARN / ERROR / CRITICAL");

// --- 模型通用参数 ---
DEFINE_double(temperature, 0.7,
    "模型温度参数，取值范围 [0.0, 2.0]");
DEFINE_int32(max_tokens, 2048,
    "每次响应的最大 token 数");

// --- Ollama 可选参数 ---
DEFINE_string(ollama_end_point, "",
    "Ollama 服务地址，例如 http://localhost:11434（可选）");
DEFINE_string(ollama_module_name, "",
    "Ollama 模型名称（可选）");
DEFINE_string(ollama_module_desc, "",
    "Ollama 模型描述（可选）");

// --- 版本号 ---
// --version 和 -v 在 main 中手动处理，不通过 gflags 定义
// 避免与 gflags 内部符号冲突

// ============================================================
// 2. 参数校验函数
// ============================================================

/// 检查温度值是否在合法范围内
static bool IsValidTemperature(double value)
{
    return value >= 0.0 && value <= 2.0;
}

/// 检查 max_tokens 是否为非负数
static bool IsValidMaxTokens(int32_t value)
{
    return value >= 0;
}

/// 检查端口号是否合法
static bool IsValidPort(int32_t value)
{
    return value > 0 && value <= 65535;
}

/// 检查日志级别字符串是否合法
static bool IsValidLogLevel(const std::string& level)
{
    return level == "TRACE" || level == "DEBUG" || level == "INFO"
        || level == "WARN" || level == "ERROR" || level == "CRITICAL";
}

/// 将日志级别字符串转为 spdlog 枚举
static spdlog::level::level_enum ParseLogLevel(const std::string& level)
{
    if (level == "TRACE")    return spdlog::level::trace;
    if (level == "DEBUG")    return spdlog::level::debug;
    if (level == "INFO")     return spdlog::level::info;
    if (level == "WARN")     return spdlog::level::warn;
    if (level == "ERROR")    return spdlog::level::err;
    if (level == "CRITICAL") return spdlog::level::critical;
    return spdlog::level::info;
}

/// 检查 Ollama 参数：三个必须同时为空或同时非空
static bool IsValidOllamaConfig()
{
    bool epEmpty = FLAGS_ollama_end_point.empty();
    bool mnEmpty = FLAGS_ollama_module_name.empty();
    bool mdEmpty = FLAGS_ollama_module_desc.empty();

    // 全为空 → 可选，不注册 Ollama
    if (epEmpty && mnEmpty && mdEmpty) return true;
    // 全非空 → 有效
    if (!epEmpty && !mnEmpty && !mdEmpty) return true;
    // 部分填写 → 非法
    return false;
}

// ============================================================
// 3. 主函数
// ============================================================

int main(int argc, char** argv)
{
    // 3.1 设置帮助信息
    std::string usage =
R"(AIChatServer - 多模型 AI 对话服务器

用法:
  ./AIChatServer [选项...]
  ./AIChatServer --flagfile=ChatServer.conf
  ./AIChatServer --flagfile=ChatServer.conf --port=9090

参数说明:
)";
    // 3.0 手动处理 --version / -v，必须在 ParseCommandLineFlags 之前
    // 因为 gflags 不认识 --version 会报错
    // 同时避免与 gflags 内部 FLAGS_version 符号冲突
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "--version" || arg == "-v") {
            std::cout << "AIChatServer version 1.0.0" << std::endl;
            return 0;
        }
        // 如果遇到 --flagfile=xxx，不需要处理，gflags 会解析
    }

    gflags::SetUsageMessage(usage);
    gflags::ParseCommandLineFlags(&argc, &argv, true);


    // 3.3 参数安全检查
    bool hasError = false;

    if (!IsValidPort(FLAGS_port)) {
        std::cerr << "[ERROR] 端口号不合法: " << FLAGS_port
                  << "，必须为 1~65535" << std::endl;
        hasError = true;
    }

    if (!IsValidTemperature(FLAGS_temperature)) {
        std::cerr << "[ERROR] 温度值不合法: " << FLAGS_temperature
                  << "，必须为 [0.0, 2.0]" << std::endl;
        hasError = true;
    }

    if (!IsValidMaxTokens(FLAGS_max_tokens)) {
        std::cerr << "[ERROR] max_tokens 不能为负数: "
                  << FLAGS_max_tokens << std::endl;
        hasError = true;
    }

    if (!IsValidLogLevel(FLAGS_log_level)) {
        std::cerr << "[ERROR] 日志级别不合法: " << FLAGS_log_level
                  << "，可选: TRACE / DEBUG / INFO / WARN / ERROR / CRITICAL"
                  << std::endl;
        hasError = true;
    }

    if (!IsValidOllamaConfig()) {
        std::cerr << "[ERROR] Ollama 参数必须同时提供或同时为空"
                  << std::endl;
        std::cerr << "        需要同时设置: --ollama_end_point,"
                  << " --ollama_module_name, --ollama_module_desc"
                  << std::endl;
        hasError = true;
    }

    // 3.4 检查 API Key（从环境变量读取）
    std::string deepseekKey = std::getenv("deepseekapikey") ?: "";
    std::string glmKey      = std::getenv("glmapikey") ?: "";
    std::string kimiKey     = std::getenv("kimiapikey") ?: "";

    if (deepseekKey.empty() && glmKey.empty() && kimiKey.empty()) {
        std::cerr << "[ERROR] 至少需要一个 API Key（DeepSeek / GLM / Kimi）"
                  << std::endl;
        std::cerr << "        请设置环境变量: deepseekapikey, glmapikey, kimiapikey"
                  << std::endl;
        hasError = true;
    }

    if (hasError) {
        std::cerr << "\n使用 " << argv[0]
                  << " --help 查看帮助" << std::endl;
        return 1;
    }

    // 3.5 初始化日志
    bite::Logger::InitLogger("AIChatServer", "stdout", ParseLogLevel(FLAGS_log_level));

    // 3.6 构建配置
    AI_Chat_Server::ChatServerConfig config;
    config._host         = FLAGS_host;
    config._port         = FLAGS_port;
    config._logLevel     = FLAGS_log_level;
    config._temperature  = FLAGS_temperature;
    config._maxTokens    = FLAGS_max_tokens;
    config._deepSeekApiKey = deepseekKey;
    config._glmApiKey      = glmKey;
    config._kimiApiKey     = kimiKey;

    // Ollama — 仅在全部非空时填入
    if (!FLAGS_ollama_end_point.empty()) {
        config._end_point    = FLAGS_ollama_end_point;
        config._module_name  = FLAGS_ollama_module_name;
        config._module_dec   = FLAGS_ollama_module_desc;
    }

    // 3.7 启动服务器
    AI_Chat_Server::ChatServer server(config);
    if (!server.start()) {
        ERROR("ChatServer 启动失败");
        return 1;
    }

    INFO("ChatServer 已启动，监听 {}:{}", config._host, config._port);
    INFO("已注册模型: deepseek-v4-flash, glm-4.7-flash, Pro/moonshotai/Kimi-K2.6");

    // 3.8 等待停止信号
    std::cout << "\n按 Ctrl+C 停止服务器..." << std::endl;

    // 使用信号量等待，支持后台 nohup 运行
    static volatile sig_atomic_t keepRunning = 1;
    signal(SIGINT, [](int) { keepRunning = 0; });
    signal(SIGTERM, [](int) { keepRunning = 0; });

    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    server.stop();
    INFO("ChatServer 已停止");
    return 0;
}
