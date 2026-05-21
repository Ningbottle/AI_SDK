#include "../include/SessionManager.h"
#include "../include/util/myLog.h"
#include <algorithm>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <ctime>


namespace AI_Chat_SDK
{
    std::string SessionManager:: GenerateSessionId()
    {
        _sessionCounter.fetch_add(1);
        std::time_t now = std::time(nullptr);
        std::ostringstream oss;
        oss << "Session_" << now << "_" << std::setw(8) << std::setfill('0') << _sessionCounter;
        return oss.str();
    }

    std::string SessionManager::GenerateMessageId(size_t messageCounter)
    {
        messageCounter++;
        std::time_t now = std::time(nullptr);
        std::ostringstream oss;
        oss << "Message_" << now << "_" << std::setw(8) << std::setfill('0') << messageCounter;
        return oss.str();
    }

    std::string SessionManager::CreateSession(std::string modelName)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        Session session(modelName);
        session._session_id = GenerateSessionId();
        _sessions[session._session_id] = std::make_shared<Session>(session);
        return session._session_id;
    }

    bool SessionManager::DeleteSession(const std::string& SessionId)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _sessions.find(SessionId);
        if (it != _sessions.end()) {
            _sessions.erase(it);
            return true;
        }
        return false;
    }
    // 2. 根据会话id获取会话消息
    std::shared_ptr<Session> SessionManager::GetSession(const std::string& SessionId) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _sessions.find(SessionId);
        if (it != _sessions.end()) {
            return it->second;
        }
        return nullptr;
    }
    // 3. 展示会话列表：
    std::vector<std::string> SessionManager::GetSessionList() const
    {
        std::unique_lock<std::mutex> lock(_mutex);
        std::vector<std::pair<std::string,std::shared_ptr<Session>>> temp;
         temp.reserve(_sessions.size());
        for (const auto& pair : _sessions) {
            temp.push_back(pair);
        }
        std::sort(temp.begin(), temp.end(), [](const std::pair<std::string,std::shared_ptr<Session>>& a, const std::pair<std::string,std::shared_ptr<Session>>& b) {
            return a.second->_updateAt > b.second->_updateAt;
        });
        std::vector<std::string> sessionList;
        for (const auto& pair : temp) {
            sessionList.push_back(pair.first);
        }
        return sessionList;
    }
    // 4. 向会话中添加新的消息：
    bool SessionManager::AddMessage(const std::string& SessionId,const Message& message) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _sessions.find(SessionId);
        if(it == _sessions.end()) return false;
        Message msg(message._role,message._content);
        it->second->_messages.push_back(msg);
        it->second->_updateAt = std::time_t(nullptr);
        INFO("AddMessage: SessionId={}, message={}", SessionId, message._content);
        return true;
    }
    // 5. 跟新会话的时间：
    void SessionManager::UpdateSessionTimeStamp(const std::string& SessionId)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _sessions.find(SessionId);
        if(it != _sessions.end()) it->second->_updateAt = std::time_t(nullptr);
    }
    // 6. 删除所有的会话记录
    bool SessionManager::ClearAllSession()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _sessions.clear();
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
        return _sessions.size();
    }

}
