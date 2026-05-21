#include "../include/OllamalProvider.h"
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
    bool OllamalProvider::Init(const std::map<std::string, std::string>& config)
    {
        auto it = config.find("model_name");
        if (it == config.end())
        {
            ERROR("model_name not found in config");
            return false;
        }
        _modelName = it->second;
        it = config.find("endpoint");
        if (it == config.end())
        {
            ERROR("endpoint not found in config");
            return false;
        }
        _Endpoint = it->second;
        it = config.find("model_desc");
        if (it == config.end())
        {
            ERROR("model_desc not found in config");
            return false;
        }
        _modelDesc = it->second;
        _IsAvailable = true;
        return true;
    }

    bool OllamalProvider::IsAvailable() const { return _IsAvailable; }
    std::string OllamalProvider::GetModelName() const { return _modelName; }
    std::string OllamalProvider::ModelDesc() const { return _modelDesc; }
    std::string OllamalProvider::SendMessage(std::vector<Message> messages,
        std::map<std::string, std::string> requestParam)
    {
        if (!_IsAvailable)
        {
            ERROR("OllamalProvider is not available");
            return "";
        }
        //开始配置参数：
        float temperature = 0.7f;
        int max_tokens = 1024;
        auto it = requestParam.find("temperature");
        if (it != requestParam.end())
            temperature = std::stod(it->second);
        it = requestParam.find("max_tokens");
        if (it != requestParam.end())
            max_tokens = std::stoi(it->second);
        //消息记录，和构建请求体：
        Json::Value requestBody;
        requestBody["model"] = _modelName;
        requestBody["messages"] = Json::Value(Json::arrayValue);
        for (const auto& msg : messages)
        {
            Json::Value message;
            message["role"] = msg._role;
            message["content"] = msg._content;
            requestBody["messages"].append(message);
        }
        Json::Value option(Json::objectValue);  // 创建一个空的 options 对象,类型为object
        option["temperature"] = temperature;    // 设置温度参数
        option["num_ctx"] = max_tokens;         // 设置上下文长度参数
        requestBody["options"] = option;        // 将 options 对象添加到请求体中
        requestBody["stream"] = false;          // 设置 stream 参数为 false
        Json::StreamWriterBuilder builder;
        std::string requestBodyStr = Json::writeString(builder, requestBody);
        //构建客户端：
        httplib::Client client(_Endpoint.c_str());
        client.set_read_timeout(30,0);
        httplib::Headers headers = {
            {"Content-Type", "application/json"}
        };
        auto response = client.Post("/api/chat", headers, requestBodyStr, "application/json");
        if(!response)
        {
            ERROR("OllamaLLMProvider::sendMessage: failed to send request, error: {}", to_string(response.error()));
            return "";
        }
        if(response->status != 200)
        {
            ERROR("OllamaLLMProvider::sendMessage: failed to send request, status: {}", response->status);
            return "";
        }
        if(response->body.empty())
        {
            ERROR("OllamaLLMProvider::sendMessage: empty response body");
            return "";
        }
        Json::Value jsonResponse;
        Json::CharReaderBuilder readerBuilder;
        std::string error;
        std::stringstream ss(response->body);
        if(!Json::parseFromStream(readerBuilder, ss, &jsonResponse, &error))
        {
            ERROR("OllamaLLMProvider::sendMessage: failed to parse response body, error: {}", error);
            return "";
        }
        if(jsonResponse.isMember("message") && jsonResponse["message"].isObject()
            && jsonResponse["message"].isMember("content"))
        {
            return jsonResponse["message"]["content"].asString();
        }
        ERROR("OllamaLLMProvider::sendMessage: invalid response format");
        return "";
    }

    std::string OllamalProvider::SendMessageStream(std::vector<Message> messages,
                                std::map<std::string, std::string> requestParam,
                                std::function<void(std::string, bool)> callback)
    {
        if(!IsAvailable()) return "";
        float temperature = 0.7f;
        int max_tokens = 1024;
        auto it = requestParam.find("temperature");
        if (it != requestParam.end())
            temperature = std::stod(it->second);
        it = requestParam.find("max_tokens");
        if (it != requestParam.end())
            max_tokens = std::stoi(it->second);
        //消息记录，和构建请求体：
        Json::Value requestBody;
        requestBody["model"] = _modelName;
        requestBody["messages"] = Json::Value(Json::arrayValue);
        for (const auto& msg : messages)
        {
            Json::Value message;
            message["role"] = msg._role;
            message["content"] = msg._content;
            requestBody["messages"].append(message);
        }
        Json::Value option(Json::objectValue);  // 创建一个空的 options 对象,类型为object
        option["temperature"] = temperature;    // 设置温度参数
        option["num_ctx"] = max_tokens;         // 设置上下文长度参数
        requestBody["options"] = option;        // 将 options 对象添加到请求体中
        requestBody["stream"] = true;          // 设置 stream 参数为 false
        Json::StreamWriterBuilder builder;
        std::string requestBodyStr = Json::writeString(builder, requestBody);
        httplib::Headers headers =
        {
            {"Content-Type", "application/json"},
            {"Accept", "text/event-stream"}
        };

        bool IsComplete = false;
        std::string buff;
        std::string fullResponse;
        bool gotErr = false;
        std::string ErrMsg;
        httplib::Request request;
        request.method = "POST";
        request.path = "/api/chat";
        request.headers = headers;
        request.body = requestBodyStr;
        request.response_handler = [&](const httplib::Response& res) {
            if (res.status != 200) {
                gotErr = true;
                ErrMsg = res.body;
                ERROR("OllamaLLMProvider::sendMessage: status {}, body:{} ", res.status, ErrMsg);
                return false;
            }
            return false;
        };

        request.content_receiver = [&](const char *data, size_t len, uint64_t offset,
                           uint64_t total_length)
        {
            if(gotErr) return false;
            buff.append(buff,len);
            size_t pos = 0;
            while((pos = buff.find("\n")) != std::string::npos)
            {
                std::string line = buff.substr(0, pos);
                buff.erase(0, pos + 1);
                if(line.empty()) continue;
                Json::Value root;
                std::stringstream ss(line);
                std::string err;
                Json::CharReaderBuilder builder;
                if(!Json::parseFromStream(builder, ss, &root, &err))
                {
                    ERROR("Failed to parse JSON: {}", err);
                    continue;
                }
                if (root.isMember("message") && root["message"].isObject()
                           && root["message"].isMember("content"))
                {
                    std::string content = root["message"]["content"].asString();
                    if (!content.empty())
                    {
                        fullResponse += content;
                        callback(content, false);
                    }
                }
                if (root.get("done", false).asBool())
                {
                    IsComplete = true;
                    callback("", true);
                    return false;
                }
            }
            return true;
        };
        httplib::Client client(_Endpoint.c_str());
        client.set_connection_timeout(30,0);
        client.set_read_timeout(200,0);
        auto res = client.send(request);
        if(!res)
        {
            ERROR("Failed to send request: {}", httplib::to_string(res.error()));
            return "";
        }
        if(!IsComplete)
        {
            ERROR("OllamaLLMProvider::sendMessageStream: stream not finish, fullResponse: {}", fullResponse);
            callback("", true);
        }
        return fullResponse;
    }


} // namespace AI_SDK
