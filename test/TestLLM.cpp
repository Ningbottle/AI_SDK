#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <spdlog/common.h>
#include "../SDK/include/DeepSeekProvider.h"
#include "../SDK/include/GLMProvider.h"
#include "../SDK/include/KimiProvider.h"
#include "../SDK/include/util/myLog.h"
#include "../SDK/include/common.h"
#include "../SDK/include/ChatSDK.h"

// 这个是谷歌测试工具的宏，注意Test记得大写
#if 0
TEST(DeepSeekProviderTest,SendMessage)
{
    auto provider = std::make_shared<AI_Chat_SDK::DeepSeekProvider>();
    // 如果是不为空的话，就断言为正
    ASSERT_TRUE(provider != nullptr);
    std::map<std::string,std::string> config;
    config["api_key"] = getenv("DEEPSEEK_API_KEY");
    config["base_url"] = "https://api.deepseek.com";
    provider->Init(config);
    // 断言provider可用
    ASSERT_TRUE(provider->IsAvailable());
    std::map<std::string,std::string> params;
    params["temperature"] = "0.6";
    params["max_output_tokens"] = "2048";
    std::vector<AI_Chat_SDK::Message> messages;
    messages.push_back({"user", "你是谁"});

    std::string responce = provider->SendMessageStream(messages,params,
        [&](std::string content, bool isComplete)
        {
            INFO("stream content: {}", content.c_str());
            if(isComplete) INFO("[DONE]");
        });

    INFO("{}",responce.c_str());
    // 断言响应不为空
    ASSERT_FALSE(responce.empty());
}
TEST(GLMProviderTest,SendMessage)
{
    auto provider = std::make_shared<AI_Chat_SDK::GLMProvider>();
    // 如果是不为空的话，就断言为正
    ASSERT_TRUE(provider != nullptr);
    std::map<std::string,std::string> config;
    config["api_key"] = getenv("GLM_API_KEY");
    config["base_url"] = "https://open.bigmodel.cn"; //https://open.bigmodel.cn/api/paas/v4
    provider->Init(config);
    // 断言provider可用
    ASSERT_TRUE(provider->IsAvailable());
    std::map<std::string,std::string> params;
    params["temperature"] = "0.6";
    params["max_output_tokens"] = "2048";
    std::vector<AI_Chat_SDK::Message> messages;
    messages.push_back({"user", "你是谁"});

    std::string responce = provider->SendMessageStream(messages,params,
        [&](std::string content, bool isComplete)
        {
            INFO("stream content: {}", content.c_str());
            if(isComplete) INFO("[DONE]");
        });
    INFO("{}",responce.c_str());
    //std::string responce = provider->SendMessage(messages,params);
    // 断言响应不为空
    INFO("{}",responce.c_str());
    ASSERT_FALSE(responce.empty());
}

TEST(KimiProviderTest,SendMessage)
{
    auto provider = std::make_shared<AI_Chat_SDK::KimiProvider>();
    // 如果是不为空的话，就断言为正
    ASSERT_TRUE(provider != nullptr);
    std::map<std::string,std::string> config;
    config["api_key"] = "";
    config["base_url"] = "https://api.siliconflow.cn";
    provider->Init(config);
    // 断言provider可用
    ASSERT_TRUE(provider->IsAvailable());
    std::map<std::string,std::string> params;
    params["temperature"] = "0.6";
    params["max_tokens"] = "2048";
    std::vector<AI_Chat_SDK::Message> messages;
    messages.push_back({"user", "你是谁"});

    std::string responce = provider->SendMessageStream(messages,params,
        [&](std::string content, bool isComplete)
        {
            INFO("stream content: {}", content.c_str());
            if(isComplete) INFO("[DONE]");
        });
    INFO("{}",responce.c_str());
    //std::string responce = provider->SendMessage(messages,params);
    // 断言响应不为空
    //INFO("{}",responce.c_str());
    ASSERT_FALSE(responce.empty());
}
#endif



TEST(ChatSDKTest, sendMessage)
{
    auto sdk = std::make_shared<AI_Chat_SDK::ChatSDK>("test_chat.db");
    ASSERT_TRUE(sdk != nullptr);   // 确保sdk初始化成功,不等于为真，则可以通过
    // 1.先进行测试apiconfig 中的deepseek
    auto deepseekConfig = std::make_shared<AI_Chat_SDK::ApiConfig>();
    ASSERT_TRUE(deepseekConfig != nullptr); // 确保deepseekConfig初始化成功
    deepseekConfig->_module_name = "deepseek-v4-flash";
    deepseekConfig->_apiKey = ::getenv("deepseekapikey");
    ASSERT_FALSE(deepseekConfig->_apiKey.empty()); // 只有不为空才能通过
    deepseekConfig->_temperature = 0.7;
    deepseekConfig->_max_tokens = 2048;

    // 2. 在初始化glm的模型
    auto glmConfig = std::make_shared<AI_Chat_SDK::ApiConfig>();
    glmConfig->_module_name = "glm-4.7-flash";
    glmConfig->_apiKey = ::getenv("glmapikey");
    ASSERT_FALSE(glmConfig->_apiKey.empty()); // 只有不为空才能通过
    glmConfig->_temperature = 0.7;
    glmConfig->_max_tokens = 2048;

    // 3. 初始化kimi大模型:
    auto kimiConfig = std::make_shared<AI_Chat_SDK::ApiConfig>();
    kimiConfig->_module_name = "Pro/moonshotai/Kimi-K2.6";
    kimiConfig->_apiKey = ::getenv("kimiapikey");
    ASSERT_FALSE(kimiConfig->_apiKey.empty()); // 只有不为空才能通过
    kimiConfig->_temperature = 0.7;
    kimiConfig->_max_tokens = 2048;

    std::vector<std::shared_ptr<AI_Chat_SDK::Config>> cofings = {
        deepseekConfig,
        {glmConfig},
        {kimiConfig},
    };
    sdk->InitAllModels(cofings);
    auto sessionid = sdk->CreateSession(deepseekConfig->_module_name);
    std::string message;
    std::cout << " ----- 请输入 -----" << std::endl;
    std::getline(std::cin, message);
    auto res = sdk->SendMessage(sessionid, message);
    ASSERT_FALSE(res.empty()); //只有不为空才能通过
    std::cout << " ----- 回复 -----" << std::endl;
    std::cout << res << std::endl;
    std::cout << "------ 请再次输入----------" ;
    std::getline(std::cin, message);
    res = sdk->SendMessage(sessionid, message);
    ASSERT_FALSE(res.empty()); //只有不为空才能通过
    std::cout << " ----- 回复 -----" << std::endl;
    std::cout << res << std::endl;
}


int main(int argc, char **argv)
{
    bite::Logger::InitLogger("TestLLM", "stdout", spdlog::level::info);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
