#include "../include/DeepSeekProvider.h"
#include <jsoncpp/json/config.h>
#include <jsoncpp/json/forwards.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>
#include "../include/common.h"
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
    std::string DeepSeekProvider::GetModelName() const  { return "deepseek-v4-flash";}

    std::string DeepSeekProvider::SendMessage(std::vector<Message> messages,
                    std::map<std::string, std::string> requestParam)
    {
        // 1. 检测模型是否进行初始化了
        if(!_IsAvailable) return "";
        // 2. 构造请求参数：模型名称、消息列表、温度值、maxtoen数、是否开启流式响应(默认未开启)
        double temperature = 0.7;
        int max_output_tokens = 4096;
        if(requestParam.find("temperature") != requestParam.end())
            temperature = std::stod(requestParam["temperature"]);
        if(requestParam.find("max_tokens") != requestParam.end())
            max_output_tokens = std::stoi(requestParam["max_tokens"]);
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
        requestBody["model"] = GetModelName();
        requestBody["messages"] = MessageArray;
        requestBody["temperature"] = temperature;
        requestBody["max_tokens"] = max_output_tokens;
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
            ERROR("DeepSeekProvider sendMessage POST request failed, error: {}",httplib::to_string(res.error()));
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
        std::istringstream responseStream(res->body);
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
                                std::function<void(std::string, bool)> callback)
    {
        // 1. 检测模型是否初始化成功
        if(!IsAvailable()) {return "";}
        // 2. 从config 构建参数，如果没有参数，使用默认参数
        double temperature = 0.7;
        int max_output_tokens = 4096;
        if(requestParam.find("temperature") != requestParam.end())
            temperature = std::stod(requestParam["temperature"]);
        if(requestParam.find("max_tokens") != requestParam.end())
            max_output_tokens = std::stoi(requestParam["max_tokens"]);

        // 3. 构建请求体
        Json::Value requestBody;
        requestBody["model"] = GetModelName();
        Json::Value messageArray;
        for(const auto& message : messages)
        {
            Json::Value messageItem;
            messageItem["role"] = message._role;
            messageItem["content"] = message._content;
            messageArray.append(messageItem);
        }
        requestBody["messages"] = messageArray;
        requestBody["temperature"] = temperature;
        requestBody["max_tokens"] = max_output_tokens;
        requestBody["stream"] = true; // 启用流式输出

        // 3. 开始利用Json的write工厂序列化
        Json::StreamWriterBuilder WriteBuilder;
        WriteBuilder.settings_["indentation"] = ""; // 紧凑输出
        std::unique_ptr<Json::StreamWriter> writer(WriteBuilder.newStreamWriter());
        std::stringstream ss;
        writer->write(requestBody, &ss);
        std::string requestBodyStr = ss.str();
        INFO("DeepSeekProvider sendMessageStream requestBody: {}", requestBodyStr);
        // 4. 利用httplib来构建客户端
        httplib::Client client(_Endpoint.c_str());
        client.set_connection_timeout(30,0);
        client.set_read_timeout(300,0);
        // 5. 构建头请求
        httplib::Headers headers = {
            {"Authorization", "Bearer " + _ApiKey}, // 传入API密钥
            {"Content-Type", "application/json"},   // 发送JSON格式的数据
            {"Accept", "text/event-stream"}         // 启用流式输出
        };
        // 6.定义流式变量:
        std::string buff;           //接受流式响应的数据块
        bool gotError = false;      // 记录是否发生错误
        std::string MsgError;       // 记录错误信息
        int statusCode = 0;         // 记录响应状态码
        bool IsComplete = false;    // 记录响应是否完成
        std::string fullResponse;   // 记录完整的响应内容

        // 7.利用httplib创建响应Request对象：
        httplib::Request request;
        request.method = "POST";                // 使用POST方法
        request.path = "/v1/chat/completions";    // 发送到chat/completions接口,兼容open AI 的借口
        request.headers = headers;              // 设置请求头 在第 2 步完成
        request.body = requestBodyStr;          // 设置请求体 在第 3 步完成
        // 7.1设置Request的处理错误函数:
        request.response_handler = [&](const httplib::Response& res) {
            statusCode = res.status;
            if (res.status != 200)
            {
                gotError = true;
                MsgError = res.body;
                ERROR("Request failed with status code: {}, body: {}", res.status, MsgError);
                return false;
            }
            return true;
        };
        // 7.2 设置Request的content_receiver
        request.content_receiver = [&](const char* data, size_t len,uint64_t offset, uint64_t totalLength)
        {
            if(gotError == true) return false;
            buff.append(data, len);
            INFO("DeepSeek send Message {}", buff);
            ssize_t pos;
            while((pos = buff.find("\n\n")) != std::string::npos)
            {
                std::string chunk = buff.substr(0, pos);
                buff = buff.substr(pos + 2);
                // 忽略空行和以冒号开头的行,因为:在sse协议中是注释
                if(chunk.empty() || chunk[0] == ':') continue;
                // 获取有效数据了：data: 一共是6个字节
                if(chunk.compare(0, 6, "data: ") == 0)
                {
                    std::string modelData = chunk.substr(6);
                    if(modelData == "[DONE]")
                    {
                        callback("", true);
                        IsComplete = true;
                        return true;
                    }
                    // 开始进行反序列化
                    Json::Value modelDataJson;
                    Json::CharReaderBuilder builder;
                    std::string errors;
                    std::istringstream modelDataStream(modelData);
                    if(Json::parseFromStream(builder,modelDataStream, &modelDataJson, &errors))
                    {
                        // 开始解析
                        if(modelDataJson.isMember("choices") && modelDataJson["choices"].isArray()
                        && modelDataJson["choices"].size() > 0 && modelDataJson["choices"][0].isMember("delta")
                        && modelDataJson["choices"][0]["delta"].isMember("content"))
                        {
                            std::string content = modelDataJson["choices"][0]["delta"]["content"].asString();
                            fullResponse += content;  // 累加响应内容
                            callback(content, false); // 这里选择false 是因为后面可能还有字段
                        }
                    }
                    else    WARN("modelDataJson parse failed:{} ", errors);
                }
            }
            return true;
        };

        // 已经设置完了两个响应器，我们应该发送;
        auto result = client.send(request);
        if(result == false)         //我记得这个好像重载了bool 的比较
        {
            DEBUG("send request failed,maybe is Network: {}", to_string(result.error()));
            return "";
        }
        if(!IsComplete)  // 如果没有结束
        {
            DEBUG("stream not finish");
            callback("", true);
        }
        return fullResponse;
    }


    std::string DeepSeekProvider::ModelDesc() const
    {
        return "这是一个世界顶级的ai 助手，DeepSeek,擅长解题和代码以及逻辑推理";
    }
}
