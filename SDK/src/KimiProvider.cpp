#include "../include/KimiProvider.h"
#include <jsoncpp/json/config.h>
#include <jsoncpp/json/forwards.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/writer.h>
#include "../include/common.h"
#include <httplib.h>
#include <map>
#include <sstream>

namespace AI_Chat_SDK
{

    bool KimiProvider::Init(const std::map<std::string, std::string>& config)
    {
        auto it = config.find("api_key");
        if (it == config.end()) { return false; }
        _ApiKey = it->second;
        it = config.find("base_url");
        if (it == config.end()) { return false; }
        _Endpoint = it->second;
        _IsAvailable = true;
        INFO("KimiProvider initialized successfully, endpoint {}", _Endpoint);
        return true;
    }

    bool KimiProvider::IsAvailable() const {return _IsAvailable == true;}
    std::string KimiProvider::GetModelName() const {return "Pro/moonshotai/Kimi-K2.6";}
    std::string KimiProvider::ModelDesc() const
    {
        return "月之暗面开发的 AI 助手 Kimi，擅长长文本处理与多模态任务，致力于为你提供高效的服务";
    }

    std::string KimiProvider::SendMessage(std::vector<Message> messages,
                        std::map<std::string, std::string> requestParam)
    {
        if(!IsAvailable()) { return ""; }
        // 查找温度和max_tokens
        double temperature = 0.7;
        int max_tokens = 4096;
        auto it = requestParam.find("temperature");
        if (it != requestParam.end()) {
            temperature = std::stod(it->second);
        }
        it = requestParam.find("max_tokens");
        if (it != requestParam.end()) {
            max_tokens = std::stoi(it->second);
        }
        //构建历史消息
        Json::Value requestBody;
        Json::Value messagesArray(Json::arrayValue);
        for(auto& msg : messages)
        {
            Json::Value messageObj(Json::objectValue);
            messageObj["role"] = msg._role;
            messageObj["content"] = msg._content;
            messagesArray.append(messageObj);
        }
        requestBody["model"] = GetModelName();
        requestBody["messages"] = messagesArray;
        requestBody["temperature"] = temperature;
        requestBody["max_tokens"] = max_tokens;

        httplib::Client client(_Endpoint.c_str());
        client.set_connection_timeout(30,0);
        client.set_read_timeout(60,0);
        httplib::Headers headers = {
            {"Authorization", "Bearer " + _ApiKey},
            {"Content-Type", "application/json"},
        };
        // 进行序列化：
        Json::StreamWriterBuilder builder;
        std::string requestBodyStr = Json::writeString(builder, requestBody);
        // 可以开始发送请求了：
        auto res = client.Post("/v1/chat/completions", headers, requestBodyStr, "application/json");
        if(!res)
        {
            ERROR("KimiProvider sendMessage POST request failed, error: {}",httplib::to_string(res.error()));
            return "";
        }
        if(res->status != 200)
        {
            ERROR("status: {}, body: {}", res->status, res->body);
            return "";
        }
        Json::CharReaderBuilder readerBuilder;
        Json::Value responseBody;
        std::string err;
        std::istringstream stream(res->body);
        if(!Json::parseFromStream(readerBuilder, stream, &responseBody, &err))
        {
            ERROR("KimiProvider sendMessage JSON parse failed, error: {}",err);
            return "";
        }
        if(responseBody.isMember("choices") && !responseBody["choices"].empty())
        {
            if(responseBody["choices"][0].isMember("message") && responseBody["choices"][0]["message"].isMember("content"))
            {
                std::string content = responseBody["choices"][0]["message"]["content"].asString();
                return content;
            }
            else
            {
                ERROR("KimiProvider sendMessage JSON parse failed, no content field in response");
                return "";
            }
        }
        else
        {
            ERROR("KimiProvider sendMessage JSON parse failed, no choices field in response");
            return "";
        }
    }

    std::string KimiProvider::SendMessageStream(std::vector<Message> messages,
                                    std::map<std::string, std::string> requestParam,
                                    std::function<void(std::string, bool)>callback)
    {
        if(!IsAvailable()) { return ""; }
        // 查找温度和max_tokens
        double temperature = 0.7;
        int max_tokens = 4096;
        auto it = requestParam.find("temperature");
        if (it != requestParam.end()) {
            temperature = std::stod(it->second);
        }
        it = requestParam.find("max_tokens");
        if (it != requestParam.end()) {
            max_tokens = std::stoi(it->second);
        }
        //构建历史消息
        Json::Value requestBody;
        Json::Value messagesArray(Json::arrayValue);
        for(auto& msg : messages)
        {
            Json::Value messageObj(Json::objectValue);
            messageObj["role"] = msg._role;
            messageObj["content"] = msg._content;
            messagesArray.append(messageObj);
        }
        requestBody["model"] = GetModelName();
        requestBody["messages"] = messagesArray;
        requestBody["temperature"] = temperature;
        requestBody["max_tokens"] = max_tokens;
        requestBody["stream"] = true;
        std::string requestBodyStr = Json::writeString(Json::StreamWriterBuilder(), requestBody);
        //构建客户端：
        httplib::Client client(_Endpoint.c_str());
        client.set_read_timeout(200,0);
        client.set_connection_timeout(30,0);
        httplib::Headers headers =
        {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + _ApiKey},
            {"Accept", "text/event-stream"}
        };
        httplib::Request request;
        request.method = "POST";
        request.path = "/v1/chat/completions";
        request.body = requestBodyStr;
        request.headers = headers;
        //设置错误处理函数：
        std::string buff;           //接受流式响应的数据块
        bool gotError = false;      // 记录是否发生错误
        std::string MsgError;       // 记录错误信息
        int statusCode = 0;         // 记录响应状态码
        bool IsComplete = false;    // 记录响应是否完成
        std::string fullResponse;   // 记录完整的响应内容
        request.response_handler = [&](const httplib::Response& res)
        {
            if (res.status != 200)
            {
                gotError = true;
                MsgError = res.body;
                statusCode = res.status;
                ERROR("HTTP status: {}, body:{} ", statusCode, MsgError);
                return false;
            }
            return true;
        };

        request.content_receiver = [&](const char *data, size_t len, uint64_t offset,
                           uint64_t totalLength)
        {
            if(gotError == true) return false;
            buff.append(data, len);
            INFO("Kimi Send Msg {}", buff);
            size_t pos = 0;
            while((pos = buff.find("\n\n")) != std::string::npos)
            {
                std::string line = buff.substr(0, pos);
                buff.erase(0, pos + 2);
                if(line.empty() || line[0] == ':') continue;
                if(line.compare(0,6,"data: ") == 0)
                {
                    // 如果出现了data: 则解析数据
                    std::string modelData = line.substr(6);
                    if(modelData.compare(0, 5, "[DONE]") == 0)
                    {
                        callback("", true);
                        IsComplete = true;
                        return false;
                    }
                    Json::CharReaderBuilder builder;
                    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
                    Json::Value root;
                    std::string errs;
                    if(reader->parse(modelData.c_str(), modelData.c_str() + modelData.size(), &root, &errs))
                    {
                        if(root.isMember("choices") && root["choices"].isArray() && root["choices"].size() > 0
                            && root["choices"][0].isMember("delta"))
                        {
                            Json::Value& delta = root["choices"][0]["delta"];

                            // 优先取 content（可见文本），为空时取 reasoning_content（思考链）
                            std::string content;
                            if (delta.isMember("content") && !delta["content"].isNull())
                                content = delta["content"].asString();
                            else if (delta.isMember("reasoning_content") && !delta["reasoning_content"].isNull())
                                content = delta["reasoning_content"].asString();

                            if (!content.empty())
                            {
                                callback(content, false);
                                // 只有可见 content 才收入 fullResponse，reasoning 不保存
                                if (delta.isMember("content") && !delta["content"].isNull())
                                    fullResponse += content;
                                return true;
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
}
