#include <string>
#include <vector>
#include <map>
#include <functional>
#include "common.h"
#include "./util/myLog.h"

namespace AI_Chat_SDK {
    class LLMProvider {
    public:
        LLMProvider();
        virtual ~LLMProvider();
        // 1. 初始化模型提供者，使用配置参数
        virtual bool Init(const std::map<std::string, std::string>& config) = 0;
        // 2. 是否初始化完成
        virtual bool IsAvailable() const = 0;
        // 3. 获取模型名称
        virtual std::string GetModeName() const = 0;
        // 4. 发送消息，非流式，就是非增量 第一个参数是消息，第二个参数是温度，最大tokens，之类的
        virtual void SendMessage(std::vector<Message> messages,
                            std::map<std::string, std::string> requestParam) = 0;
        // 5. 发送消息，流式: 第一个参数是消息，第二个参数是温度，最大tokens，之类的，第三个参数是回调函数
        // 回到函数里面是第一个是增量，第二个是否还有增量
        virtual void SendMessageStream(std::vector<Message> messages,
                                    std::map<std::string, std::string> requestParam,
                                    std::function<void(std::string, bool)>) = 0;
        virtual std::string ModelDesc() const = 0;
    protected:
        bool _IsAvailable = false;
        std::string _ApiKey;        // Api key 用于模型通话
        std::string _Endpoint;      // URL of 模型通讯连接地址
    };
} // namespace AI_chat
