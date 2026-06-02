#include "../include/ChatSDK.h"
#include "../include/DeepSeekProvider.h"
#include "../include/GLMProvider.h"
#include "../include/OllamalProvider.h"
#include <asm-generic/errno.h>
#include <memory>
#include <regex>
#include <unordered_set>

namespace AI_Chat_SDK
{
    ChatSDK::ChatSDK(std::string dbName)
        : _SessionManager(dbName)
    {
    }

    void ChatSDK::RegisterAllProviders(const std::vector<std::shared_ptr<Config>>& configs)
    {
        if(!_LLMManager.isModelAvailable("deepseek-v4-flash"))
        {
            //如果DeepSeek不存在就开始注册：
            _LLMManager.registerModel("deepseek-v4-flash", std::make_unique<DeepSeekProvider>());
            INFO("DeepSeekProvider registered successfully.");
        }
        if(!_LLMManager.isModelAvailable("glm-4.7-flash"))
        {
            _LLMManager.registerModel("glm-4.7-flash", std::make_unique<GLMProvider>());
            INFO("GLMProvider registered successfully.");
        }
        if(!_LLMManager.isModelAvailable("Pro/moonshotai/Kimi-K2.6"))
        {
            _LLMManager.registerModel("Pro/moonshotai/Kimi-K2.6", std::make_unique<GLMProvider>());
            INFO("Kimi-K2.6Provider registered successfully.");
        }
        //最后初始化ollama模型：
        std::unordered_set<std::string> ModuleName; //防止被重复注入
        for(const auto& config : configs)
        {
            auto ollamaConfig = std::dynamic_pointer_cast<OllamaConfig>(config);
            if(ollamaConfig)
            {
                auto name = ollamaConfig->_module_name;
                if(ModuleName.find(name) == ModuleName.end())
                    ModuleName.insert(name);
                if(!_LLMManager.isModelAvailable(name))
                {
                    _LLMManager.registerModel(name,std::make_unique<OllamalProvider>());
                    INFO("OllamaLLMProvider {} registered successed",name);
                }
            }
        }
    }
    bool ChatSDK::InitApiModules(std::string moduleName,const std::shared_ptr<ApiConfig>& configs)
    {
        //初始化这些模型，已经确定了是ApiConfig了，那么接下来是：
        if(moduleName.empty())
        {
            ERROR("moduleName is empty");
            return false;
        }
        if(!configs || configs->_apiKey.empty())
        {
            ERROR("configs is nullptr");
            return false;
        }
        if(_LLMManager.isModelAvailable(moduleName))
        {
            INFO("ChatSDK::InitApiModules Model {} is already available", moduleName);
            return true;
        }
        std::map<std::string, std::string> moduleParams;
        moduleParams["api_key"] = configs->_apiKey;
        if(!_LLMManager.initModel(moduleName, moduleParams))
        {
            ERROR("Failed to init model {}", moduleName);
            return false;
        }
        _moduleConfigs[moduleName] = configs;
        return true;
    }
    bool ChatSDK::InitOllamaModules(std::string moduleName,const std::shared_ptr<OllamaConfig>& configs)
    {
        if(moduleName.empty())
        {
            ERROR("moduleName is empty");
            return false;
        }
        if(!configs || configs->_end_point.empty())
        {
            ERROR("configs is nullptr or end_point is empty");
            return false;
        }
        if(_LLMManager.isModelAvailable(moduleName))
        {
            INFO("ChatSDK::InitOllamaModules Model {} is already available", moduleName);
            return true;
        }
        std::map<std::string, std::string> modelParams;
        modelParams["model_name"] = configs->_module_name;
        modelParams["endpoint"] = configs->_end_point;
        modelParams["model_desc"] = configs->_module_dec;
        _LLMManager.initModel(moduleName, modelParams);
        _moduleConfigs[moduleName] = configs;
        return true;
    }
    void ChatSDK::InitAllProviders(const std::vector<std::shared_ptr<Config>>& configs)
    {
        for(const auto& config : configs)
        {
            auto apiConfig = std::dynamic_pointer_cast<ApiConfig>(config);
            if(apiConfig)
            {
                if(apiConfig->_module_name == "deepseek-v4-flash" || apiConfig->_module_name == "glm-4.7-flash"
                    || apiConfig->_module_name == "Pro/moonshotai/Kimi-K2.6")
                    InitApiModules(apiConfig->_module_name, apiConfig);
                else
                    ERROR("Unsupported API module: {}", apiConfig->_module_name);
            }
            else if(auto ollamaConfig = std::dynamic_pointer_cast<OllamaConfig>(config))
                InitOllamaModules(ollamaConfig->_module_name, ollamaConfig);
            else
                ERROR("Unsupported config type: {}", config->_module_name);
        }
    }

    bool ChatSDK::InitAllModels(const std::vector<std::shared_ptr<Config>>& configs)
    {
        RegisterAllProviders(configs);
        InitAllProviders(configs);
        _IsInitialized = true;
        return true;
    }

    std::string ChatSDK::CreateSession(const std::string &modelName)
    {
        if (_IsInitialized == false)
        {
            ERROR("ChatSDK is not initialized");
            return "";
        }
        std::string sessionId = _SessionManager.CreateSession(modelName);
        if(sessionId.empty())
        {
            ERROR("Failed to create session for model: {}", modelName);
            return "";
        }
        return sessionId;
    }

    std::vector<std::string> ChatSDK::GetSessionList()
    {
        if (!_IsInitialized)
        {
            ERROR("ChatSDK is not initialized");
            return {};
        }
        return _SessionManager.GetSessionList();
    }

    std::shared_ptr<Session> ChatSDK::GetSession(const std::string& sessionId)
    {
        if (!_IsInitialized)
        {
            ERROR("ChatSDK is not initialized");
            return nullptr;
        }
        auto session = _SessionManager.GetSession(sessionId);
        if (!session)
        {
            ERROR("Session not found: {}", sessionId);
            return nullptr;
        }
        return session;
    }

    bool ChatSDK::DeleteSession(const std::string& sessionId)
    {
        if (!_IsInitialized)
        {
            ERROR("ChatSDK is not initialized");
            return false;
        }
        bool ret = _SessionManager.DeleteSession(sessionId);
        if (!ret)
        {
            ERROR("Failed to delete session: {}", sessionId);
        }
        return ret;
    }

    std::vector<ModuleInfo> ChatSDK::GetAvailableModels()
    {
        return _LLMManager.getAvailableModels();
    }

    std::string ChatSDK::SendMessage(const std::string& sessionId, const std::string& message)
    {
        if (!_IsInitialized)
        {
            ERROR("ChatSDK is not initialized");
            return "";
        }
        auto Session = _SessionManager.GetSession(sessionId);
        if (!Session)
        {
            ERROR("Session not found: {}", sessionId);
            return "";
        }
        Message userMessage("user", message);
        _SessionManager.AddMessage(sessionId, userMessage);
        std::vector<Message> messages = _SessionManager.GetMessage(sessionId);
        std::map<std::string, std::string> requestParam;
        auto it = _moduleConfigs.find(Session->_model_name);
        if(it != _moduleConfigs.end())
        {
            requestParam["temperature"] = std::to_string(it->second->_temperature);
            requestParam["max_tokens"] = std::to_string(it->second->_max_tokens);
        }
        std::string response = _LLMManager.sendMessage(Session->_model_name, messages, requestParam);
        if(response.empty())
        {
            ERROR("sendMessage failed: {}", response);
            return "";
        }
        Message assistantMessage("assistant", response);
        _SessionManager.AddMessage(sessionId, assistantMessage);
        _SessionManager.UpdateSessionTimeStamp(sessionId);
        INFO("ChatSDK::sendMessage: send message to model {} successed", Session->_model_name);
        return response;
    }

    std::string ChatSDK::sendMessageStream(const std::string &sessionId, const std::string &message,
        std::function<void (const std::string &, bool)> callback)
    {
        if (!_IsInitialized)
        {
            ERROR("ChatSDK is not initialized");
            return "";
        }
        auto Session = _SessionManager.GetSession(sessionId);
        if (!Session)
        {
            ERROR("Session not found: {}", sessionId);
            return "";
        }
        Message userMessage("user", message);
        _SessionManager.AddMessage(sessionId, userMessage);
        std::vector<Message> messages = _SessionManager.GetMessage(sessionId);
        std::map<std::string, std::string> requestParam;
        auto it = _moduleConfigs.find(Session->_model_name);
        if(it != _moduleConfigs.end())
        {
            requestParam["temperature"] = std::to_string(it->second->_temperature);
            requestParam["max_tokens"] = std::to_string(it->second->_max_tokens) ;
        }
        std::string response = _LLMManager.sendMessageStream(Session->_model_name, messages, requestParam, callback);
        if(response.empty())
        {
            ERROR("sendMessageStream failed: {}", response);
            return "";
        }
        Message assistantMessage("assistant", response);
        _SessionManager.AddMessage(sessionId, assistantMessage);
        _SessionManager.UpdateSessionTimeStamp(sessionId);
        INFO("ChatSDK::sendMessageStream: send message to model {} successed", Session->_model_name);
        return response;
    }
};
