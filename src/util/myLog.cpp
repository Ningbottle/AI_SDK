#include "../../include/util/myLog.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <mutex>

namespace bite {
    // 对于两个static 函数都需要在源文件中进行初始化
    std::shared_ptr<spdlog::logger> Logger::_logger = nullptr;
    std::mutex Logger::_mutex;

    // Logger的构造函数 — 由于Logger是一个纯静态工具类（所有成员都是static），
    // 此构造函数没有实际用途。通常应将其设为private或delete，以防止实例化。
    // 当前保留空实现以维持编译通过。
    Logger::Logger() {} // 这是什么？

    void Logger::InitLogger(const std::string logName, const std::string FilePath,
                        spdlog::level::level_enum logLevel) {
        if(_logger == nullptr)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if(_logger == nullptr)
            {
                // 1. 设置刷新策略，当 > 该默认策略的时候开始刷新
                spdlog::flush_on(logLevel);
                // 2. 启动异步打印策略： 将日志消息推送到后台线程进行处理，避免阻塞主线程
                spdlog::init_thread_pool(32789,1); //参数1：消息日志多少，参数2：一个线程
                if(FilePath == "stdout") _logger = spdlog::stdout_color_mt(logName);
                // 由于这是在线程池中，采用异步的spdlog::async_factory更加合适
                else _logger = spdlog::basic_logger_mt<spdlog::async_factory>(logName, FilePath);
            }
        }
        // 3.格式设置:
        // 3.1. [%H:%M:%S] 时分秒
        // 3.2. [%n] 日志名称
        // 3.3. [%-7l] 日志级别  左对齐,7个宽度 %v 日志内容
        _logger->set_pattern("[%H:%M:%S][%n][%-7l] %v");
        _logger->set_level(logLevel); // 设置了任务等级
    }

    // 为什么这里需要返回spdlog::logger?
    // 因为spdlog::logger是一个智能指针，返回它可以方便地在其他地方使用,但是为什么不是我们封装的对象Logger呢？
    std::shared_ptr<spdlog::logger> Logger::getLogger() {
        return _logger;
    }
}
