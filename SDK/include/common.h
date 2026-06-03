#pragma once
#include <ctime>
#include <string>
#include <vector>

namespace AI_Chat_SDK {
// 1.消息会话管理
struct Message {
  // 一条消息需要什么，应该怎吗改变：
  std::string _message_id;        // 消息id
  std::string _role;      //扮演的角色
  std::string _content;   // 消息内容
  std::time_t _timestamp; //时间戳

  Message(std::string role = "", std::string content = "")
      : _role(role), _content(content) {}
};

// 2. 有了消息就要开始配置模型公共参数：
struct Config {
  std::string _module_name; //模型名称
  double _temperature = 0.7; //温度，低表示适合代码，高代表很适合创作
  int _max_tokens = 2048; //最大输入tokens

  virtual ~Config() {}
};

// 2.1 通过apikey接入模型
struct ApiConfig :public Config{
  std::string _apiKey; //接入模型的api key
  std::string _baseUrl; // API 地址（可选，不填则用 Provider 默认）
};

// 2.2 通过Ollama接入模型
struct OllamaConfig :public Config{
  std::string _module_dec; //模型的描述
  std::string _end_point;  // 接入模型的 url;
};

// 3.模型信息
struct ModuleInfo {
  std::string _model_name;   // 模型名称
  std::string _model_desc;   // 模型描述
  std::string _provider;     // 模型提供者
  std::string _endpoint;     // 模型API endpoint  base url
  bool _isAvailable = false; // 模型是否可用

  ModuleInfo(std::string moduleName = "", std::string modelDesc = "",
             std::string provider = " ", std::string endpoint = "")
      : _model_name(moduleName), _model_desc(modelDesc), _provider(provider),
        _endpoint(endpoint) {}
};

// 4. 会话结构
struct Session {
  std::string _session_id;                //消息id
  std::string _model_name;        // 模型的名称
  std::vector<Message> _messages; // 消息的管理
  std::time_t _createAt;          // 会话的保存时间
  std::time_t _updateAt;          //最新的会话保存时间
  Session(std::string modelName = "") : _model_name(modelName) {}
};
}; // namespace AI_chat
