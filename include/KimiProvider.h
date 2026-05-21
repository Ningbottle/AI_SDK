#pragma once
#include "LLMProvider.h"

namespace AI_Chat_SDK
{
    class KimiProvider : public LLMProvider
    {
    public:
        ~KimiProvider() = default;
        // 1. 初始化模型提供者，使用配置参数
        virtual bool Init(const std::map<std::string, std::string>& config) override;
        // 2. 是否初始化完成
        virtual bool IsAvailable() const override;
        // 3. 获取模型名称
        virtual std::string GetModelName() const override;
        // 4. 发送消息，非流式，就是非增量 第一个参数是消息，第二个参数是温度，最大tokens，之类的
        virtual std::string SendMessage(std::vector<Message> messages,
                            std::map<std::string, std::string> requestParam) override;
        // 5. 发送消息，流式: 第一个参数是消息，第二个参数是温度，最大tokens，之类的，第三个参数是回调函数
        // 回到函数里面是第一个是增量，第二个是否还有增量
        virtual std::string SendMessageStream(std::vector<Message> messages,
                                    std::map<std::string, std::string> requestParam,
                                    std::function<void(std::string, bool)>) override;
        // 6. 获取模型描述
        virtual std::string ModelDesc() const override;
    };
}
