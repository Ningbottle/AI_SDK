#pragma once
#include "common.h"
#include <cstddef>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <mutex>
#include <memory>

namespace AI_Chat_SDK
{
    class DataManager
    {
    private:
        sqlite3* _db = nullptr;     // 数据库指针
        std::string _dbName;        // 数据库名字
        mutable std::mutex _mutex;  // 互斥锁
    private:
        bool InitDB();
        bool executeSQL(const std::string& sql);
        std::shared_ptr<Session> GetSessionByIdNoLock(const std::string& sessionId);
        bool updateSessionStateNoLock(const std::shared_ptr<Session>& session, std::time_t TimeStamp);
        std::vector<Message> GetMessagesBySessionIdNolock(const std::string& sessionId);

    public:
        DataManager(const std::string& dbName);
        ~DataManager();
        //关于会话的方面的
        std::vector<std::string> GetAllSessionId();// 获取所有会话的id
        std::shared_ptr<Session> GetSessionById(const std::string& sessionId);// 根据会话id获取会话
        bool insertSession(const std::shared_ptr<Session>& session);// 插入会话
        bool updateSessionState(const std::shared_ptr<Session>& session, std::time_t TimeStamp);// 更新会话状态和时间戳
        bool deleteSession(const std::string& sessionId);// 删除会话
        std::size_t GetSessionCount();
        std::vector<std::shared_ptr<Session>> GetAllSessions();
        bool clearAllSessions();
        // 关于消息方面的：
        bool insertMessage(const std::string& sessionId, const Message& message, std::time_t timestamp); // 插入消息
        std::vector<Message> GetMessagesBySessionId(const std::string& sessionId); // 根据会话id获取消息
        bool deleteSessionMessage(const std::string& sessionId); // 删除会话所有的消息
    };
}
