#include <cstdio>
#include <gtest/gtest.h>
#include <spdlog/common.h>
#include "../include/DeepSeekProvider.h"
#include "../include/GLMProvider.h"
#include "../include/KimiProvider.h"
#include "../include/util/myLog.h"
#include "../include/common.h"

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
#endif





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


int main(int argc, char **argv)
{
    bite::Logger::InitLogger("TestLLM", "stdout", spdlog::level::info);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
