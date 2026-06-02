#include "../include/SessionManager.h"
#include "../include/util/myLog.h"
#include <algorithm>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <ctime>


namespace AI_Chat_SDK
{
    SessionManager::SessionManager(std::string dbName) :
        _dataManager(dbName)
    {
        // 1. 从数据库加载所有会话到内存，保持内存与 DB 的初始一致性
        auto sessions = _dataManager.GetAllSessions();
        for (const auto& session : sessions) {
            _sessions[session->_session_id] = std::make_shared<Session>(*session);
        }
    }

    std::string SessionManager:: GenerateSessionId()
    {
        // 1. 自增计数器，生成全局唯一的会话 ID
        _sessionCounter.fetch_add(1);
        std::time_t now = std::time(nullptr);
        std::ostringstream oss;
        oss << "Session_" << now << "_" << std::setw(8) << std::setfill('0') << _sessionCounter;
        return oss.str();
    }

    std::string SessionManager::GenerateMessageId(size_t messageCounter)
    {
        // 1. 自增计数器，生成全局唯一的消息 ID
        messageCounter++; // FIXME: messageCounter 是传值的，外部不受影响，建议后续改成成员变量计数器
        std::time_t now = std::time(nullptr);
        std::ostringstream oss;
        oss << "Message_" << now << "_" << std::setw(8) << std::setfill('0') << messageCounter;
        return oss.str();
    }

    // 1. 创建会话和删除会话：
    std::string SessionManager::CreateSession(std::string modelName)
    {
        // 1. 锁内：生成 ID、构造 Session 对象、写入内存
        std::shared_ptr<Session> session;
        std::string sessionId;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            sessionId = GenerateSessionId();
            session = std::make_shared<Session>(modelName);
            session->_session_id = sessionId;
            session->_createAt = std::time(nullptr);
            session->_updateAt = session->_createAt;
            _sessions[sessionId] = session;
        }   // 锁在这里释放
        // 2. 锁外：写入数据库，避免在持有锁时做 IO
        _dataManager.insertSession(session);
        return sessionId;
    }

    bool SessionManager::DeleteSession(const std::string& SessionId)
    {
        // 1. 锁内：从内存中删除，拷贝出 sessionId
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _sessions.find(SessionId);
            if (it == _sessions.end()) {
                return false;
            }
            _sessions.erase(it);
        }   // 锁在这里释放
        // 2. 锁外：从数据库中删除
        // NOTE: messages 表有 ON DELETE CASCADE，删除 session 时自动级联删除 messages
        _dataManager.deleteSession(SessionId);
        return true;
    }
    // 2. 根据会话id获取会话消息
    std::shared_ptr<Session> SessionManager::GetSession(const std::string& SessionId)
    {
        // 1. 锁内：从内存中找到会话，拷贝 shared_ptr
        std::shared_ptr<Session> session;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _sessions.find(SessionId);
            if (it != _sessions.end()) {
                session = it->second;
            }
        }   // 锁在这里释放
        // 2. 锁外：从数据库中加载消息列表
        if (session) {
            session->_messages = _dataManager.GetMessagesBySessionId(SessionId);
        }
        return session;
    }
    // 3. 展示会话列表：
    std::vector<std::string> SessionManager::GetSessionList() const
    {
        // 由于已经加载在内存里面了，是否考虑还需要加强优化呢？
        std::unique_lock<std::mutex> lock(_mutex);
        std::vector<std::pair<std::string,std::shared_ptr<Session>>> temp;
         temp.reserve(_sessions.size());
        for (const auto& pair : _sessions) {
            temp.push_back(pair);
        }
        std::sort(temp.begin(), temp.end(),
            [](const std::pair<std::string,std::shared_ptr<Session>>& a,
                const std::pair<std::string,std::shared_ptr<Session>>& b) {
            return a.second->_updateAt > b.second->_updateAt;
        });
        std::vector<std::string> sessionList;
        for (const auto& pair : temp) {
            sessionList.push_back(pair.first);
        }
        return sessionList;
    }
    // 4. 向会话中添加新的消息：
    bool SessionManager::AddMessage(const std::string& SessionId,const Message& message)
    {
        // 1. 锁内：找到会话、构造消息、写入内存
        std::shared_ptr<Session> session;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _sessions.find(SessionId);
            if (it == _sessions.end()) {
                return false;
            }
            Message msg(message._role, message._content);
            msg._message_id = GenerateMessageId(it->second->_messages.size());
            it->second->_messages.push_back(msg);
            it->second->_updateAt = std::time(nullptr);
            session = it->second; // 锁内拷贝 shared_ptr
        }   // 锁在这里释放
        // 2. 锁外：写入数据库（insertMessage 内部同时更新了 sessions 表的 updated_time）
        _dataManager.insertMessage(SessionId, message, session->_updateAt);
        INFO("AddMessage: SessionId={}, message={}", SessionId, message._content);
        return true;
    }
    // 5. 跟新会话的时间：
    void SessionManager::UpdateSessionTimeStamp(const std::string& SessionId)
    {
        // 1. 锁内：更新内存中的时间戳，拷贝 shared_ptr 留待后用
        std::shared_ptr<Session> session;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _sessions.find(SessionId);
            if (it != _sessions.end()) {
                it->second->_updateAt = std::time(nullptr);
                session = it->second; // 这里不能用迭代器，因为下面解锁后迭代器会失效
            }
        }   // 锁在这里释放
        // 2. 锁外：更新数据库中的时间戳
        // 这里使用拷贝出来的 session，而不是 it->second，因为锁已经释放了
        if (session) {
            _dataManager.updateSessionState(session, session->_updateAt);
        }
    }
    // 6. 删除所有的会话记录
    bool SessionManager::ClearAllSession()
    {
        // 1. 锁内：清空内存
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _sessions.clear();
        }   // 锁在这里释放
        // 2. 锁外：清空数据库
        _dataManager.clearAllSessions();
        return true;
    }
    // 7. 获取id会话中的消息列表
    std::vector<Message> SessionManager:: GetMessage(const std::string& SessionId) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _sessions.find(SessionId);
        if(it == _sessions.end()) return {};
        std::vector<Message> messages = it->second->_messages;
        return messages;
    }
    std::size_t SessionManager::getSessionCount()const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _sessions.size();
    }

}
