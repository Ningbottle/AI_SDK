#pragma once
#include "LLMProvider.h"
#include "common.h"
#include <memory>
#include <vector>

namespace AI_Chat_SDK
{
    class LLMManager
    {
    public:
        // 既然是模型的管理者，那么就要注册模型：
        bool registerModel(const std::string& modelName, std::unique_ptr<LLMProvider> llmProvider);
        // 初始化模型：
        bool initModel(const std::string& modelName,const std::map<std::string, std::string>& param);
        // 检测模型是否可用
        bool isModelAvailable(const std::string& modelName) const;
        // 获取可用的模型：
        std::vector<ModuleInfo> getAvailableModels() const;
        // 发送消息非流式：
        std::string sendMessage(const std::string& modelName, const std::vector<Message> message,
            const std::map<std::string, std::string>& requestParam);
        // 发送消息流式：
        std::string sendMessageStream(const std::string& modelName, const std::vector<Message> message,
            const std::map<std::string, std::string>& requestParam,
            std::function<void(const std::string&,bool)> onChunk);

    private:
        std::map<std::string, std::unique_ptr<LLMProvider>> _llmProviders;
        std::map<std::string, ModuleInfo> _moduleInfo;
    };
}
