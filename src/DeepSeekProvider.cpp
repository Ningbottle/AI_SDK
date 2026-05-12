#include "../include/DeepSeekProvider.h"
#include <jsoncpp/json/config.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>
#include <httplib.h>
#include <sstream>

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

    std::string DeepSeekProvider::SendMessage(std::vector<Message> messages,
                    std::map<std::string, std::string> requestParam)
    {
        // 1. 检测模型是否进行初始化了
        if(!_IsAvailable) return "";
        // 2. 构造请求参数：模型名称、消息列表、温度值、maxtoen数、是否开启流式响应(默认未开启)
        double temperature = 0.7;
        int maxTokens = 4096;
        if(requestParam.find("temperature") != requestParam.end())
            temperature = std::stod(requestParam["temperature"]);
        if(requestParam.find("max_output_tokens") != requestParam.end())
            maxTokens = std::stoi(requestParam["max_output_tokens"]);
        //      2.1 构建消息列表
        Json::Value MessageArray(Json::arrayValue);
        for(const auto& msg : messages)
        {
            Json::Value message;
            message["role"] = msg._role;
            message["content"] = msg._content;
            MessageArray.append(message);
        }
        //      2.2 构建请求体
        Json::Value requestBody;
        requestBody["model"] = GetModeName();
        requestBody["messages"] = MessageArray;
        requestBody["temperature"] = temperature;
        requestBody["max_output_tokens"] = maxTokens;
        // 3. 序列化：
        Json::StreamWriterBuilder WriteBuilder;
        std::string requestBodyStr = Json::writeString(WriteBuilder, requestBody);
        // 4. 创建客户端，使用第三方httplib：
        httplib::Client client(_Endpoint.c_str());
        client.set_connection_timeout(30,0);
        client.set_read_timeout(60,0);
        httplib::Headers headers = {
            {"Authorization", "Bearer " + _ApiKey},
            {"Content-Type", "application/json"}
        };
        // 5. 发送请求 ,第一个参数是：请求路径，第二个参数是:请求头，第三个参数是:请求体，第四个参数是:请求体类型
        auto res = client.Post("/chat/completions", headers, requestBodyStr, "application/json");
        if(!res)
        {
            ERROR("DeepSeekProvider sendMessage POST request failed, error: {}",res.error());
            return "";
        }
        // 6. 检测是否响应成功：状态不对就开始
        if(res->status != 200)
        {
            ERROR("DeepSeekProvider sendMessage POST request failed, status: {}", res->status);
            return "";
        }
        INFO("DeepSeekProvider API reponse body : {}", res->body);
        // 7. 解析响应体
        Json::Value root;
        Json::CharReaderBuilder readerBuilder;
        Json::istreamstring responseStream(res->body);
        std::string errorJson;
        Json::Value requestJson;
        if(!Json::parseFromStream(readerBuilder,responseStream, &requestJson, &errorJson))
        {
            ERROR("DeepSeekProvider sendMessage POST request failed, parse response body failed, error: {}"
                , errorJson);
            return "";
        }
        // 8. 提取响应内容
        std::string responseContent = requestJson["choices"][0]["message"]["content"].asString();
        return responseContent;
    }

    std::string DeepSeekProvider::SendMessageStream(std::vector<Message> messages,
                                std::map<std::string, std::string> requestParam,
                                std::function<void(std::string, bool)>)
    {
        return "";
    }

    std::string DeepSeekProvider::ModelDesc() const
    {
        return "这是一个世界顶级的ai 助手，DeepSeek,擅长解题和代码以及逻辑推理";
    }
}
