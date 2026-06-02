#pragma once
#include <httplib.h>
#include <ai_chat_sdk/ChatSDK.h>
#include <memory>


namespace AI_Chat_Server{
struct ChatServerConfig
{
    std::string _host = "0.0.0.0";
    int _port = 8080;

    // 设置一下日志的等级：
    std::string _logLevel = "INFO";

    // 设置 module config
    double _temperature = 0.7;
    int _maxTokens = 2048;

    //设置 api config
    std::string _deepSeekApiKey;
    std::string _glmApiKey;
    std::string _kimiApiKey;
    std::string _deepSeekApiUrl = "https://api.deepseek.com";
    std::string _glmApiUrl = "https://open.bigmodel.cn";
    std::string _kimiApiUrl = "https://api.siliconflow.cn";

    // 设置 ollama config
    std::string _module_dec; //模型的描述
    std::string _end_point;  // 接入模型的 url;
    std::string _module_name; // 模型的名称
};

class ChatServer
{
private:
    std::unique_ptr<httplib::Server> _ChatServer = nullptr;
    std::unique_ptr<AI_Chat_SDK::ChatSDK> _ChatSDK = nullptr;
    ChatServerConfig _config;
    std::atomic<bool> _isRunning = {false};
private:
    std::string HandleErrJson(std::string message);
    // 1. 处理创建会话的请求：
    void handleCreateSessionRequest(const httplib::Request& request, httplib::Response& response);
    // 2. 处理获取会列表的请求：
    void handleGetSessionListRequest(const httplib::Request& request, httplib::Response& response);
    // 3. 处理获取模型列表的请求:
    void handleGetModelListRequest(const httplib::Request& request, httplib::Response& response);
    // 4. 处理删除会话的请求：
    void handleDeleteSessionRequest(const httplib::Request& request, httplib::Response& response);
    // 5. 处理获取历史消息的请求：
    void handleGetHistoryRequest(const httplib::Request& request, httplib::Response& response);
    // 6. 处里非流式的发送消息：
    void handleSendMessageRequest(const httplib::Request& request, httplib::Response& response);
    // 7. 处理流式的发送消息：
    void handleSendStreamMessageRequest(const httplib::Request& request, httplib::Response& response);
    void setHttpRoutes();
public:
    ChatServer(const ChatServerConfig& config);
    bool start();
    bool stop();
    bool isRunning();

};
}
