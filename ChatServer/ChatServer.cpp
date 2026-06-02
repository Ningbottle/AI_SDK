#include "ChatServer.h"
#include <cstddef>
#include <jsoncpp/json/value.h>
#include <memory>
#include <streambuf>
#include <thread>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>
#include <type_traits>


namespace AI_Chat_Server
{
    ChatServer::ChatServer(const ChatServerConfig& config)
        : _config(config)
    {
        _ChatSDK = std::make_unique<AI_Chat_SDK::ChatSDK>();
        auto DeepSeekConfig = std::make_shared<AI_Chat_SDK::ApiConfig>();
        DeepSeekConfig->_apiKey = config._deepSeekApiKey;
        DeepSeekConfig->_module_name = "deepseek-v4-flash";
        DeepSeekConfig->_max_tokens = config._maxTokens;
        DeepSeekConfig->_temperature = config._temperature;\

        auto GLMConfig = std::make_shared<AI_Chat_SDK::ApiConfig>();
        GLMConfig->_apiKey = config._glmApiKey;
        GLMConfig->_module_name = "glm-4.7-flash";
        GLMConfig->_max_tokens = config._maxTokens;
        GLMConfig->_temperature = config._temperature;

        auto kimiConfig = std::make_shared<AI_Chat_SDK::ApiConfig>();
        kimiConfig->_apiKey = config._kimiApiKey;
        kimiConfig->_module_name = "Pro/moonshotai/Kimi-K2.6";
        kimiConfig->_max_tokens = config._maxTokens;
        kimiConfig->_temperature = config._temperature;

        auto OllmaConfig = std::make_shared<AI_Chat_SDK::OllamaConfig>();
        OllmaConfig->_end_point = config._end_point;
        OllmaConfig->_module_name = config._module_name;
        OllmaConfig->_module_dec = config._module_dec;
        OllmaConfig->_max_tokens = config._maxTokens;
        OllmaConfig->_temperature = config._temperature;

        std::vector<std::shared_ptr<AI_Chat_SDK::Config>> configs = {
            DeepSeekConfig,
            GLMConfig,
            kimiConfig,
            OllmaConfig,
        };
        if( !_ChatSDK->InitAllModels(configs))
        {
            ERROR("Failed to initialize all models");
            return;
        }
        INFO("ChatSDK models init success!!!");
        _ChatServer = std::make_unique<httplib::Server>();
        if(!_ChatServer){
            ERROR("ChatServer init Failed!!!");
            return;
        }
    }

    bool ChatServer::start()
    {
        if(_isRunning.load())
        {
            ERROR("ChatServer is already running");
            return false;
        }
        setHttpRoutes();
        _ChatServer->set_mount_point("/", "./www");

        _isRunning.store(true);
        std::thread([this]() {
            _ChatServer->listen(_config._host.c_str(), _config._port);
        }).detach();
        return true;
    }

    bool ChatServer::stop()
    {
        if(!_isRunning.load())
        {
            ERROR("ChatServer is not running");
            return false;
        }
        if(_ChatServer)
        {
            _ChatServer->stop();
            _ChatServer = nullptr;
        }
        _isRunning.store(false);
        INFO("ChatServer stopped successfully");
        return true;
    }

    bool ChatServer::isRunning()
    {
        return _isRunning.load();
    }

    std::string ChatServer::HandleErrJson(std::string message)
    {
        Json::Value errJson;
        errJson["success"] = false;
        errJson["message"] = message;
        // 进行反序列化.
        Json::FastWriter writer;
        return writer.write(errJson);
    }
    void ChatServer::handleCreateSessionRequest(const httplib::Request& request, httplib::Response& response)
    {
        Json::Value requestJson;
        Json::Reader reader;
        // 解析请求的请求体：
        if(!reader.parse(request.body, requestJson))
        {
            std::string errJsonstr = HandleErrJson("parse request body failed, json format error");
            response.status = 400;
            response.set_content(errJsonstr, "application/json");
            return;
        }
        std::string moduleName = requestJson.get("moduleName", "deepseek-v4-falsh").asString();
        std::string sessionId = _ChatSDK->CreateSession(moduleName);
        if(sessionId.empty())
        {
            std::string errJsonstr = HandleErrJson("create session failed");
            response.status = 500;
            response.set_content(errJsonstr, "application/json");
            return;
        }
        Json::Value dataJson;
        dataJson["model"] = moduleName;
        dataJson["session_id"] = sessionId;
        Json::Value responseJson;
        responseJson["success"] = true;
        responseJson["message"] = "create session success";
        responseJson["data"] = dataJson;
        std::string responseJsonStr = Json::FastWriter().write(responseJson);
        response.status = 200;
        response.set_content(responseJsonStr, "application/json");
    }
    void ChatServer::handleGetSessionListRequest(const httplib::Request& request, httplib::Response& response)
    {
        std::vector<std::string> sessionList = _ChatSDK->GetSessionList();
        Json::Value dataJson(Json::arrayValue);
        for(std::string sessionId : sessionList)
        {
            auto session = _ChatSDK->GetSession(sessionId);
            if(session)
            {
                Json::Value sessionJson;
                sessionJson["id"] = sessionId;
                sessionJson["model"] = session->_model_name;
                sessionJson["created_at"] = static_cast<int64_t>(session->_createAt);
                sessionJson["updated_at"] = static_cast<int64_t>(session->_updateAt);
                sessionJson["message_count"] = session->_messages.size();
                // 获取第一条消息:
                if(!session->_messages.empty())
                {
                    sessionJson["first_user_message"] = session->_messages.front()._content;
                }
                dataJson.append(sessionJson);
            }
        }
        Json::Value responseJson;
        responseJson["success"] = true;
        responseJson["message"] = "get session list success";
        responseJson["data"] = dataJson;
        std::string responseJsonStr = Json::FastWriter().write(responseJson);
        response.status = 200;
        response.set_content(responseJsonStr, "application/json");
    }
    void ChatServer::handleGetModelListRequest(const httplib::Request& request, httplib::Response& response)
    {
        std::vector<AI_Chat_SDK::ModuleInfo> modelList = _ChatSDK->GetAvailableModels();
        Json::Value dataJson(Json::arrayValue);
        for(AI_Chat_SDK::ModuleInfo model : modelList)
        {
            Json::Value modelJson;
            modelJson["name"] = model._model_name;
            modelJson["desc"] = model._model_desc;
            dataJson.append(modelJson);
        }
        Json::Value responseJson;
        responseJson["success"] = true;
        responseJson["message"] = "get model list success";
        responseJson["data"] = dataJson;
        std::string responseJsonStr = Json::FastWriter().write(responseJson);
        response.status = 200;
        response.set_content(responseJsonStr, "application/json");
    }
    void ChatServer::handleDeleteSessionRequest(const httplib::Request& request, httplib::Response& response)
    {
        std::string sessionId = request.matches[1];
        if(sessionId.empty())
        {
            response.status = 400;
            response.set_content(HandleErrJson("session_id is empty"), "application/json");
            return;
        }
        bool result = _ChatSDK->DeleteSession(sessionId);
        if(!result)
        {
            response.status = 404;
            response.set_content(HandleErrJson("delete session failed"), "application/json");
            return;
        }
        Json::Value responseJson;
        responseJson["success"] = true;
        responseJson["message"] = "delete session success";
        std::string responseJsonStr = Json::FastWriter().write(responseJson);
        response.status = 200;
        response.set_content(responseJsonStr, "application/json");
    }
    void ChatServer::handleGetHistoryRequest(const httplib::Request& request, httplib::Response& response)
    {
        std::string sessionId = request.matches[1];
        auto session = _ChatSDK->GetSession(sessionId);
        if(!session)
        {
            response.status = 404;
            response.set_content(HandleErrJson("session not found"), "application/json");
            return;
        }
        Json::Value dataJson(Json::arrayValue);
        for(auto message : session->_messages)
        {
            Json::Value messageJson;
            messageJson["id"] = message._message_id;
            messageJson["role"] = message._role;
            messageJson["content"] = message._content;
            messageJson["timestamp"] = static_cast<int64_t>(message._timestamp);
            dataJson.append(messageJson);
        }
        Json::Value responseJson;
        responseJson["success"] = true;
        responseJson["message"] = "get history messages success";
        responseJson["data"] = dataJson;
        std::string responseJsonStr = Json::FastWriter().write(responseJson);
        response.status = 200;
        response.set_content(responseJsonStr, "application/json");
    }
    void ChatServer::handleSendMessageRequest(const httplib::Request& request, httplib::Response& response)
    {
        Json::Value requestJson;
        Json::Reader reader;
        if(!reader.parse(request.body, requestJson))
        {
            response.status = 400;
            response.set_content(HandleErrJson("invalid request body"), "application/json");
            return;
        }
        // 解析请求体中的 sessionId 和 message
        std::string sessionId = requestJson.get("sessionId", "").asString();
        std::string message = requestJson.get("message", "").asString();
        if(sessionId.empty() || message.empty())
        {
            response.status = 400;
            response.set_content(HandleErrJson("sessionId and message are required"), "application/json");
            return;
        }
        auto assistantMessage = _ChatSDK->SendMessage(sessionId, message);
        if(assistantMessage.empty())
        {
            response.status = 500;
            response.set_content(HandleErrJson("send message failed"), "application/json");
            return;
        }
        // 开始构建Json responcestr
        Json::Value dataJson;
        dataJson["session_id"] = sessionId;
        dataJson["response"] = assistantMessage;
        dataJson["data"]["assistant_message"] = assistantMessage;
        Json::Value responseJson;
        responseJson["success"] = true;
        responseJson["message"] = "send message success";
        responseJson["data"] = dataJson;
        Json::StreamWriterBuilder writerBuilder;
        std::string responseJsonStr = Json::writeString(writerBuilder, responseJson);
        response.status = 200; // 成功
        response.set_content(responseJsonStr, "application/json");
    }

    void ChatServer::handleSendStreamMessageRequest(const httplib::Request& request, httplib::Response& response)
    {
        Json::Value requestJson;
        Json::Reader reader;
        if(!reader.parse(request.body, requestJson))
        {
            response.status = 400;
            response.set_content(HandleErrJson("invalid request body"), "application/json");
            return;
        }
        std::string sessionId = requestJson["session_id"].asString();
        std::string message = requestJson["message"].asString();
        if(sessionId.empty() || message.empty())
        {
            response.status = 400;
            response.set_content(HandleErrJson("session_id or message is empty"), "application/json");
            return;
        }
        response.status = 200;
        response.set_header("Cache-Control", "no-cache");   // 禁用缓存
        response.set_header("Connection", "keep-alive");   // 保持连接
        response.set_header("Access-Control-Allow-Origin", "*");        // 允许跨域请求
        response.set_header("Access-Control-Allow-Headers", "*");      // 允许所有请求头

        response.set_chunked_content_provider("text/event-stream", [this, sessionId, message](size_t offset,httplib::DataSink& dataSink)->bool {
            auto WriteChunk = [&](const std::string& chunk, bool last) {
                std::string event = "data: " + chunk + "\n\n";
                dataSink.write(event.c_str(), event.size());
                if (last) {
                    std::string end = "data: [DONE]\n\n";
                    dataSink.write(end.c_str(), end.size());
                    dataSink.done();
                    return false;
                }
                return true;
            };
            // 由于处理时间比较麻烦，还有就是先发一个空格
            WriteChunk(" ", false);
            _ChatSDK->sendMessageStream(sessionId, message, WriteChunk);
            return true;
        });
    }

    void ChatServer::setHttpRoutes(){
        // 处理创建会话请求
        _ChatServer->Post("/api/session", [this](const httplib::Request& request, httplib::Response& response){
            handleCreateSessionRequest(request, response);
        });
        // 处理获取会话列表请求
        _ChatServer->Get("/api/sessions", [this](const httplib::Request& request, httplib::Response& response){
            handleGetSessionListRequest(request, response);
        });

        // 处理获取模型列表请求
         _ChatServer->Get("/api/models", [this](const httplib::Request& request, httplib::Response& response){
             handleGetModelListRequest(request, response);
        });

        // 处理删除会话请求
         _ChatServer->Delete("/api/session/(.*)", [this](const httplib::Request& request, httplib::Response& response){
            handleDeleteSessionRequest(request, response);
        });

        // 处理获取历史消息请求
         _ChatServer->Get("/api/session/(.*)/history", [this](const httplib::Request& request, httplib::Response& response){
             handleGetHistoryRequest(request, response);
        });
        // 处理发送消息请求-全量返回
        _ChatServer->Post("/api/message", [this](const httplib::Request& request, httplib::Response& response){
            handleSendMessageRequest(request, response);
        });

        // 处理发送消息请求-增量返回
        _ChatServer->Post("/api/message/async", [this](const httplib::Request& request, httplib::Response& response){
            handleSendStreamMessageRequest(request, response);
        });
    }
}
