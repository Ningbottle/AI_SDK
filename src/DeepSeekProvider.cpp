#include "../include/DeepSeekProvider.h"


namespace AI_Chat_SDK {
    bool DeepSeekProvider::Init(const std::map<std::string, std::string>& config)
    {
        auto it = config.find("api_key");
        if (it == config.end()) return false;
        _ApiKey = it->second;
        it = config.find("base_url");
        if (it == config.end()) _Endpoint = "https://api.deepseek.com";
        else _Endpoint = it->second;
        _IsAvailable = true;
        INFO("DeepSeekProvider initModel success, endpoint: {}",_Endpoint);
        return true;
    }

    bool DeepSeekProvider:: IsAvailable() const { return _IsAvailable;}
    std::string DeepSeekProvider::GetModeName() const  { return "deepseek-v4-flash";}
    void DeepSeekProvider::SendMessage(std::vector<Message> messages,
                    std::map<std::string, std::string> requestParam)
    {

    }

    void DeepSeekProvider::SendMessageStream(std::vector<Message> messages,
                                std::map<std::string, std::string> requestParam,
                                std::function<void(std::string, bool)>)
    {

    }

    std::string DeepSeekProvider::ModelDesc() const
    {
        return "这是一个世界顶级的ai 助手，DeepSeek,擅长解题和代码以及逻辑推理";
    }
}
