#pragma once
#include "common.h"
#include "LLMManager.h"
#include "SessionManager.h"
#include <unordered_map>

namespace AI_Chat_SDK
{
    class ChatSDK
    {
    public:
        ChatSDK(std::string dbName = "chatDB.db");
        // 1. 初始化所有模型
        bool InitAllModels(const std::vector<std::shared_ptr<Config>>& configs);
        // 2. 创建会话
        std::string CreateSession(const std::string& modelName);
        // 3.获取所有的会话列表
        std::vector<std::string> GetSessionList();
        // 4. 获取置顶的会话：
        std::shared_ptr<Session> GetSession(const std::string& sessionId);
        // 5. 删除会话：
        bool DeleteSession(const std::string& sessionId);
        // 6. 获取可用的模型
         std::vector<ModuleInfo> GetAvailableModels();
        // 7.发送消息： 分成
        std::string SendMessage(const std::string& sessionId, const std::string& message);
        std::string sendMessageStream(const std::string& sessionId, const std::string& message,
                                            std::function<void(const std::string&, bool)> callback);
    private:
        // 1. 注册所有的模型：
        void RegisterAllProviders(const std::vector<std::shared_ptr<Config>>& configs);
        // 2. 初始化所以的模型:
        void InitAllProviders(const std::vector<std::shared_ptr<Config>>& configs);
        // 3. 初始化需要api的模型：
        bool InitApiModules(std::string moduleName,const std::shared_ptr<ApiConfig>& configs);
        // 4. 初始化需要ollama的模型：
        bool InitOllamaModules(std::string moduleName,const std::shared_ptr<OllamaConfig>& configs);

    private:
        bool _IsInitialized = false;
        LLMManager _LLMManager;
        SessionManager _SessionManager;
        std::unordered_map<std::string, std::shared_ptr<Config>> _moduleConfigs;
    };
};
