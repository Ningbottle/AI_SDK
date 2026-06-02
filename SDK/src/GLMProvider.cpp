#include "../include/GLMProvider.h"
#include <jsoncpp/json/config.h>
#include <jsoncpp/json/forwards.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/writer.h>
#include "../include/common.h"
#include <httplib.h>
#include <sstream>

namespace AI_Chat_SDK {


    bool GLMProvider::Init(const std::map<std::string, std::string>& config)
    {
        auto it = config.find("api_key");
        if(it == config.end())  return false;
        _ApiKey = it->second;
        it = config.find("base_url");
        if(it == config.end())  _Endpoint = "https://open.bigmodel.cn";
        else _Endpoint = it->second;
        _IsAvailable = true;
        INFO("GLMProvider initModel success, endpoint: {}", _Endpoint);
        return true;
    }
    bool GLMProvider::IsAvailable() const { return _IsAvailable == true; }
    std::string GLMProvider::GetModelName() const { return "glm-4.7-flash"; } // 需要什么后面自己改

    std::string GLMProvider::SendMessage(std::vector<Message> messages,
                            std::map<std::string, std::string> requestParam)
    {
        if(!IsAvailable())  return ""; // 如果没有初始化成功就直接返回
        // 1. 设置温度参数和maxtoken,如果有的话是用requestParam的
        double temperature = 0.7;
        int maxOutputTokens = 4096;
        if(requestParam.find("temperature") != requestParam.end())
            temperature = std::stod(requestParam["temperature"]);
        if(requestParam.find("max_tokens") != requestParam.end())
            maxOutputTokens = std::stoi(requestParam["max_tokens"]);
        // 2. 构建消息列表和请求体
        Json::Value requestBody;
        requestBody["model"] = GetModelName();
        requestBody["messages"] = Json::arrayValue; // json数组
        for(const auto& msg : messages) {
            Json::Value message;
            message["role"] = msg._role;
            message["content"] = msg._content;
            requestBody["messages"].append(message);
        }
        requestBody["temperature"] = temperature;
        requestBody["max_tokens"] = maxOutputTokens;
        // 3.利用httplib来构建客户端
        httplib::Client client(_Endpoint.c_str());
        client.set_connection_timeout(30,0);
        client.set_read_timeout(30,0);
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + _ApiKey}
        };
        // 4. 对body进行序列化
        Json::StreamWriterBuilder builder;
        builder.settings_["indentation"] = "";
        std::string body = Json::writeString(builder, requestBody);
        // 5. 发送请求
        auto result = client.Post("/api/paas/v4/chat/completions", headers, body, "application/json");
        if(!result)
        {
            ERROR("GLMProvider sendMessage POST request failed, error: {}",httplib::to_string(result.error()));
            return "";
        }
        if(result->status != 200)
        {
            ERROR("GLMProvider sendMessage POST request failed, status: {}",result->status);
            return "";
        }
        Json::CharReaderBuilder readerbuilder;
        Json::Value requestJson;
        std::string errs;
        std::istringstream ss(result->body);
        if (!Json::parseFromStream(readerbuilder, ss, &requestJson, &errs)) {
            ERROR("parse response body failed: {}", errs);
            return "";
        }
        std::string responseContent = requestJson["choices"][0]["message"]["content"].asString();
        return responseContent;
    }

    std::string GLMProvider::SendMessageStream(std::vector<Message> messages,
                                    std::map<std::string, std::string> requestParam,
                                    std::function<void(std::string, bool)>callback)
    {
        if(!IsAvailable()) { return ""; }
        // 设置参数
        double temperature = 0.6;
        int maxTokens = 4096;
        auto it = requestParam.find("temperature");
        if(it != requestParam.end()) { temperature = std::stod(it->second); }
        it = requestParam.find("max_tokens");
        if(it != requestParam.end()) { maxTokens = std::stoi(it->second); }
        // 构建消息列表和请求体
        Json::Value requestBody;
        requestBody["model"] = GetModelName();
        Json::Value messagesArray;
        for(auto& msg : messages)
        {
            Json::Value message;
            message["role"] = msg._role;
            message["content"] = msg._content;
            messagesArray.append(message);
        }
        requestBody["messages"] = messagesArray;
        requestBody["temperature"] = temperature;
        requestBody["max_tokens"] = maxTokens;
        requestBody["stream"] = true;
        // 序列化：
        Json::StreamWriterBuilder writerBuilder;
        std::string requestBodyStr = Json::writeString(writerBuilder, requestBody);
        INFO("GLMProvider sendMessageStream requestBody: {}", requestBodyStr);
        // 构建客户端：httplib
        httplib::Client client(_Endpoint);
        client.set_connection_timeout(30,0);
        client.set_read_timeout(300,0);      //设置时间要长一些
        // 构建request 对象:
        httplib::Request request;
        request.method = "POST";
        request.path = "/api/paas/v4/chat/completions";
        request.body = requestBodyStr;
        request.set_header("Authorization", "Bearer " + _ApiKey);
        request.set_header("Content-Type", "application/json");
        request.set_header("Accept","text/event-stream");
        // 开始定义流式变量：
        std::string buff;           //接受流式响应的数据块
        bool gotError = false;      // 记录是否发生错误
        std::string MsgError;       // 记录错误信息
        int statusCode = 0;         // 记录响应状态码
        bool IsComplete = false;    // 记录响应是否完成
        std::string fullResponse;   // 记录完整的响应内容
        // 设置response的错误处理函数
        request.response_handler = [&](const httplib::Response& res) {
            statusCode = res.status;
            if(statusCode != 200)
            {
                gotError = true;
                MsgError = res.body;
                ERROR("Request failed with status code: {}, body: {}", res.status, MsgError);
                return false;
            }
            return true;
        };
        // 设置处理函数：
        request.content_receiver = [&](const char* data, size_t len,uint64_t offset, uint64_t totalLength) {
            if(gotError == true) return false;
            buff.append(data, len);
            INFO("GLM Send Msg {}", buff);
            while(buff.find("\n\n") != std::string::npos)
            {
                std::string chunk = buff.substr(0, buff.find("\n\n"));
                buff.erase(0, buff.find("\n\n") + 2);
                if(chunk.empty() || chunk[0] == ':') continue;
                if(chunk.compare(0, 6, "data: ") == 0)
                {
                    std::string modelData = chunk.substr(6);
                    if(modelData == "[DONE]")
                    {
                        callback("", true);
                        IsComplete = true;
                        return true;
                    }
                    // 开始进行反序列化:
                    Json::CharReaderBuilder readerBuilder;  //利用char reader解析json
                    Json::Value root;                       //解析结果存储在root中
                    std::string errs;                       //解析错误信息
                    std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
                    if (!reader->parse(modelData.data(), modelData.data() + modelData.size(), &root, &errs))
                    {
                        ERROR("modelDataJson parse failed:{} ", errs);
                        return false;
                    }
                    else
                    {
                        //错误：逻辑与，chunk.empty()为true时访问[0]越界
                        if(!root.isMember("choices") || root["choices"].empty()) continue;
                        Json::Value delta = root["choices"][0]["delta"];
                        // 后续我打算提取思维链，给显现,这个先不用考虑
                        if (delta.isMember("content") && !delta["content"].isNull())
                        {
                            std::string content = delta["content"].asString();
                            if(!content.empty())
                            {
                                fullResponse += content;
                                callback(content, false);
                            }
                        }
                    }
                }
            }
            return true;
        };
        auto result = client.send(request);
        if(!result)
        {
            ERROR("send request failed:{}", to_string(result.error()));
            return "";
        }
        if(!IsComplete)  // 如果没有结束
        {
            DEBUG("stream not finish");
            callback("", true);
        }
        return fullResponse;
    }

    std::string GLMProvider::ModelDesc() const
    {
        return "GLM-4.7-Flash 作为 30B 级 SOTA 模型，提供了一个兼顾性能与效率的新选择。";
    }
} // namespace AI_chat
