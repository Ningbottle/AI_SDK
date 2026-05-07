#pragma once
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>

namespace bite {
//使用日志库
class Logger {
public:
  // 主要目的是：初始化日志器
  // 第一个参数是日志器的名字，第二个参数是打印日志到哪里，第三个参数是日志的等级
  static void InitLogger(const std::string logName, const std::string FilePath,
                    spdlog::level::level_enum logLevel);
  static std::shared_ptr<spdlog::logger>getLogger(); // 获取单例, 线程安全.为什么是返回spdlog::logger?
private:
  Logger();
  // 对于单利模式的拷贝构造和赋值重载都应该取消
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

private:
  static std::shared_ptr<spdlog::logger> _logger;
  static std::mutex _mutex;
};
} // namespace bite


// 日志格式前缀: "[文件名:行号]: "
// {:>10s} 表示文件名右对齐，宽度至少10个字符（对应 __FILE__）
// {:<4d} 表示行号左对齐，宽度至少4个字符（对应 __LINE__）
// 其实本质还是调用了spdlog.但是我们保证了线程安全
#define Trace(format,...) bite::Logger::getLogger()->trace(std::string("[{:>10s}:{<4d}]: ") + format, __FILE__, __LINE__, ##__VA_ARGS__)
#define Debug(format,...) bite::Logger::getLogger()->debug(std::string("[{:>10s}:{<4d}]: ") + format, __FILE__, __LINE__, ##__VA_ARGS__)
#define Info(format,...) bite::Logger::getLogger()->info(std::string("[{:>10s}:{<4d}]: ") + format, __FILE__, __LINE__, ##__VA_ARGS__)
#define Warn(format,...) bite::Logger::getLogger()->warn(std::string("[{:>10s}:{<4d}]: ") + format, __FILE__, __LINE__, ##__VA_ARGS__)
#define Error(format,...) bite::Logger::getLogger()->error(std::string("[{:>10s}:{<4d}]: ") + format, __FILE__, __LINE__, ##__VA_ARGS__)
#define Critical(format,...) bite::Logger::getLogger()->critical(std::string("[{:>10s}:{<4d}]: ") + format, __FILE__, __LINE__, ##__VA_ARGS__)
