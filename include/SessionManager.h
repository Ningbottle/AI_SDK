#pragma once
#include <cstddef>
#include <mutex>
#include <atomic>
#include <memory>
#include <random>
#include <unordered_map>
#include "common.h"


namespace AI_Chat_SDK
{
    class SessionManager
    {
    public:
        // 1. 创建会话和删除会话：
        std::string CreateSession(std::string modelName);
        bool DeleteSession(const std::string& SessionId);
        // 2. 根据会话id获取会话消息
        std::shared_ptr<Session> GetSession(const std::string& SessionId) const;
        // 3. 展示会话列表：
        std::vector<std::string> GetSessionList() const;
        // 4. 向会话中添加新的消息：
        bool AddMessage(const std::string& SessionId,const Message& message) const ;
        // 5. 跟新会话的时间：
        void UpdateSessionTimeStamp(const std::string& SessionId);
        // 6. 删除所有的会话记录
        bool ClearAllSession();
        // 7. 获取id会话中的消息列表
        std::vector<Message> GetMessage(const std::string& SessionId) const;
        std::size_t getSessionCount()const;
    private:
        std::string GenerateSessionId();
        std::string GenerateMessageId(size_t messageCounter);
    private:
        std::unordered_map<std::string,std::shared_ptr<Session>> _sessions; // 管理会话id和会话
        mutable std::mutex _mutex; // 保护_sessions的并发访问 mutable表示const情况下是可以改变的
        std::atomic<uint64_t> _sessionCounter = {0};

    };
}
