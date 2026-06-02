#include "../include/LLMManager.h"

namespace AI_Chat_SDK
{
    bool LLMManager::registerModel(const std::string &modelName, std::unique_ptr<LLMProvider> llmProvider)
    {
        if(!llmProvider)
        {
            ERROR("llmProvider is nullptr");
            return false;
        }
        _llmProviders[modelName] = std::move(llmProvider);
        INFO("Model {} registered successfully", modelName);
        return true;
    }

    bool LLMManager::initModel(const std::string &modelName,const std::map<std::string, std::string>& param)
    {
        auto it = _llmProviders.find(modelName);
        if(it == _llmProviders.end())
        {
            ERROR("Model {} not Registered", modelName);
            return false;
        }
        bool res = it->second->Init(param);
        if(!res)
        {
            ERROR("Model {} init failed", modelName);
            return false;
        }
        _moduleInfo[modelName]._model_desc = it->second->ModelDesc();
        _moduleInfo[modelName]._model_name = modelName;
        _moduleInfo[modelName]._isAvailable = true;
        INFO("Model {} initialized successfully", modelName);
        return res;
    }

    bool LLMManager::isModelAvailable(const std::string& modelName) const
    {
        if (_llmProviders.find(modelName) == _llmProviders.end())
            return false;
        auto infoIt = _moduleInfo.find(modelName);
        return infoIt != _moduleInfo.end() && infoIt->second._isAvailable;
    }

    std::vector<ModuleInfo> LLMManager::getAvailableModels() const
    {
        std::vector<ModuleInfo> res;
        for(auto &pair : _moduleInfo)
        {
            if(pair.second._isAvailable)
            {
                res.push_back(pair.second);
            }
        }
        return res;
    }
    std::string LLMManager::sendMessage(const std::string& modelName, const std::vector<Message> message,
        const std::map<std::string, std::string>& requestParam)
    {
        auto it = _llmProviders.find(modelName);
        if(it == _llmProviders.end())
        {
            ERROR("Model {} not Registered", modelName);
            return "";
        }
        if(!_moduleInfo[modelName]._isAvailable)
        {
            ERROR("Model {} not available", modelName);
            return "";
        }
        std::string res = it->second->SendMessage(message, requestParam);
        return res;
    }

    std::string LLMManager::sendMessageStream(const std::string& modelName, const std::vector<Message> message,
        const std::map<std::string, std::string>& requestParam,
        std::function<void(const std::string&,bool)> onChunk)
    {
        auto it = _llmProviders.find(modelName);
        if(it == _llmProviders.end())
        {
            ERROR("Model {} not Registered", modelName);
            return "";
        }
        if(!_moduleInfo[modelName]._isAvailable)
        {
            ERROR("Model {} not available", modelName);
            return "";
        }
        std::string res = it->second->SendMessageStream(message, requestParam, onChunk);
        return res;
    }
}
