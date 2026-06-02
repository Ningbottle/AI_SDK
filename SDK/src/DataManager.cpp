#include "../include/DataManager.h"
#include "../include/util/myLog.h"
#include <cstdint>
#include <mutex>

namespace AI_Chat_SDK {
    DataManager::DataManager(const std::string& dbName)
        :_db(nullptr)
        ,_dbName(dbName)
    {
        int ret = sqlite3_open(_dbName.c_str(), &_db);
        if (ret != SQLITE_OK) {
            ERROR("Failed to open database: {}", sqlite3_errmsg(_db));
            sqlite3_close(_db);
            _db = nullptr;
            throw std::runtime_error("Failed to open database");
        }
        INFO("open success");
        if(!InitDB())
        {
            ERROR("Failed to init database");
            sqlite3_close(_db);
            _db = nullptr;
            throw std::runtime_error("Failed to init database");
        }
        INFO("init success");
    }

    DataManager::~DataManager()
    {
        if(_db)
        {
            sqlite3_close(_db);
            _db = nullptr;
        }
    }

    bool DataManager::executeSQL(const std::string& sql)
    {
        if(!_db) return false;
        // 执行sql语句：
        char* error = nullptr;
        int ret = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, &error);
        if(ret != SQLITE_OK)
        {
            ERROR("Failed to execute SQL: {}", error);
            sqlite3_free(error);
            return false;
        }
        return true;
    }

    bool DataManager::InitDB()
    {
        // 创建了会话表
        std::string createSessionTable = R"(
            CREATE TABLE IF NOT EXISTS sessions (
                session_id TEXT PRIMARY KEY,
                model_name TEXT NOT NULL,
                created_time INTEGER NOT NULL,
                updated_time INTEGER NOT NULL
            );
        )";
        if(!executeSQL(createSessionTable)) return false;
        // 创建了消息表
        std::string createMessageTable = R"(
            CREATE TABLE IF NOT EXISTS messages (
                message_id TEXT PRIMARY KEY,
                session_id TEXT NOT NULL,
                role TEXT NOT NULL,
                content TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                FOREIGN KEY (session_id) REFERENCES sessions(session_id) ON DELETE CASCADE
            );
        )";
        if(!executeSQL(createMessageTable)) return false;
        return true;
    }

    std::shared_ptr<Session> DataManager::GetSessionByIdNoLock(const std::string& sessionId)
    {
        std::string sql = R"(
            SELECT * FROM sessions WHERE session_id = ?
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about GetSessionById: {}", sqlite3_errmsg(_db));
            return nullptr;
        }
        // 绑定SessionId,用于查询
        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        if(sqlite3_step(stmt) != SQLITE_ROW)
        {
            sqlite3_finalize(stmt);
            return nullptr;
        }
        // 3. 成功之后开始提取有效的消息:
        const unsigned char* ModelNamePtr = sqlite3_column_text(stmt, 1);
        std::string ModelName = ModelNamePtr ? reinterpret_cast<const char*>(ModelNamePtr) : "";
        time_t CreatedAt = sqlite3_column_int64(stmt, 2);
        time_t UpdatedAt = sqlite3_column_int64(stmt, 3);
        std::shared_ptr<Session> session = std::make_shared<Session>(ModelName);
        session->_createAt = CreatedAt;
        session->_updateAt = UpdatedAt;
        session->_messages = GetMessagesBySessionIdNolock(sessionId);
        INFO("getSession - 获取会话成功：{}", sessionId);

        sqlite3_finalize(stmt);
        return session;
    }

    bool DataManager::updateSessionStateNoLock(const std::shared_ptr<Session>& session, std::time_t TimeStamp)
    {
        std::string sql = R"(
            UPDATE sessions SET updated_time = ? WHERE session_id = ?
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about updateSessionState: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_int64(stmt, 1, static_cast<uint64_t>(TimeStamp));
        sqlite3_bind_text(stmt, 2, session->_session_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE)
        {
            ERROR("Failed to execute statement about updateSessionState: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_finalize(stmt);
        return true;
    }

    std::vector<Message> DataManager::GetMessagesBySessionIdNolock(const std::string& sessionId)
    {
        std::string sql = R"(
            SELECT * FROM messages WHERE session_id = ?
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about GetMessagesBySessionId: {}", sqlite3_errmsg(_db));
            return {};
        }
        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        std::vector<Message> messages;
        messages.reserve(32); // 预分配空间，避免多次扩容
        while(sqlite3_step(stmt) == SQLITE_ROW)
        {
            Message msg( reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
            msg._message_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            messages.push_back(msg);
        }
        sqlite3_finalize(stmt);
        return messages;
    }

    std::vector<std::string> DataManager::GetAllSessionId()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string sql = R"(
            SELECT session_id FROM sessions ORDER BY updated_time DESC;
        )";
        sqlite3_stmt* stmt; // 声明一个sqlite3_stmt指针用于存储编译后的SQL语句
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr); // 编译SQL语句
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about GetAllSessionId: {}", sqlite3_errmsg(_db));
            return {};
        }
        std::vector<std::string> sessionIds;
        sessionIds.reserve(64); // 预分配空间
        if(sqlite3_step(stmt) != SQLITE_ROW)
        {
            sqlite3_finalize(stmt);
            return sessionIds;
        }
        do
        {
            const unsigned char* raw_text = sqlite3_column_text(stmt, 0);
            // 2. 判定是否为空指针
            if (raw_text != nullptr)
                sessionIds.push_back(reinterpret_cast<const char*>(raw_text)); // 非空指针，转换并添加到结果中
            else
                sessionIds.push_back(""); // 处理空指针情况
        } while(sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
        return sessionIds;
    }

    // 根据会话id获取会话
    std::shared_ptr<Session> DataManager::GetSessionById(const std::string& sessionId)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string sql = R"(
            SELECT * FROM sessions WHERE session_id = ?
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about GetSessionById: {}", sqlite3_errmsg(_db));
            return nullptr;
        }
        // 绑定SessionId,用于查询
        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        if(sqlite3_step(stmt) != SQLITE_ROW)
        {
            sqlite3_finalize(stmt);
            return nullptr;
        }
        // 3. 成功之后开始提取有效的消息:
        const unsigned char* ModelNamePtr = sqlite3_column_text(stmt, 1);
        std::string ModelName = ModelNamePtr ? reinterpret_cast<const char*>(ModelNamePtr) : "";
        time_t CreatedAt = sqlite3_column_int64(stmt, 2);
        time_t UpdatedAt = sqlite3_column_int64(stmt, 3);
        std::shared_ptr<Session> session = std::make_shared<Session>(ModelName);
        session->_createAt = CreatedAt;
        session->_updateAt = UpdatedAt;
        session->_messages = GetMessagesBySessionIdNolock(sessionId);
        INFO("getSession - 获取会话成功：{}", sessionId);

        sqlite3_finalize(stmt);
        return session;
    }

    // 插入会话
    bool DataManager::insertSession(const std::shared_ptr<Session>& session)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string sql = R"(
            INSERT INTO sessions (session_id,model_name,created_time,updated_time)
            VALUES (?,?,?,?)
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about insertSession: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, session->_session_id.c_str(),-1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, session->_model_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, static_cast<uint64_t>(session->_createAt));
        sqlite3_bind_int64(stmt, 4, static_cast<uint64_t>(session->_updateAt));
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE)
        {
            ERROR("Failed to execute statement about insertSession: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_finalize(stmt);
        return true;
    }
    // 更新会话状态和时间戳
    bool DataManager::updateSessionState(const std::shared_ptr<Session>& session, std::time_t TimeStamp)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string sql = R"(
            UPDATE sessions SET updated_time = ? WHERE session_id = ?
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about updateSessionState: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_int64(stmt, 1, static_cast<uint64_t>(TimeStamp));
        sqlite3_bind_text(stmt, 2, session->_session_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE)
        {
            ERROR("Failed to execute statement about updateSessionState: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_finalize(stmt);
        return true;
    }

    // 删除会话
    bool DataManager::deleteSession(const std::string& sessionId)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string sql = R"(
            DELETE FROM sessions WHERE session_id = ?
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about deleteSession: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE)
        {
            ERROR("Failed to execute statement about deleteSession: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        return true;
    }

    std::size_t DataManager::GetSessionCount()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string sql = R"(
            SELECT COUNT(*) FROM sessions
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about GetSessionCount: {}", sqlite3_errmsg(_db));
            return 0;
        }
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_ROW)
        {
            ERROR("Failed to execute statement about GetSessionCount: {}", sqlite3_errmsg(_db));
            return 0;
        }
        std::size_t count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return count;
    }

    std::vector<std::shared_ptr<Session>> DataManager::GetAllSessions()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string sql = R"(
           SELECT * FROM sessions ORDER BY updated_time DESC
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about GetAllSessions: {}", sqlite3_errmsg(_db));
            return {};
        }
        std::vector<std::shared_ptr<Session>> sessions;
        sessions.reserve(64);
        while((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            std::string sessionId = reinterpret_cast<const char* >(sqlite3_column_text(stmt, 0));
            std::string modelName = reinterpret_cast<const char* >(sqlite3_column_text(stmt, 1));
            std::time_t createdTime = sqlite3_column_int64(stmt, 2);
            std::time_t updatedTime = sqlite3_column_int64(stmt, 3);
            std::shared_ptr<Session> session = std::make_shared<Session>(modelName);
            session->_session_id = sessionId;
            session->_createAt = createdTime;
            session->_updateAt = updatedTime;
            sessions.push_back(session);
            // 历史消息暂时不获取，需要时再通过会话id来进行获取
        }
        sqlite3_finalize(stmt);
        INFO("getAllSessions - 获取所有会话信息成功, 会话总数：{}",sessions.size());
        return sessions;
    }

    bool DataManager::clearAllSessions()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string sql = R"(
           DELETE FROM sessions
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("clearAllSessions - 准备删除会话信息失败: {}", sqlite3_errmsg(_db));
            return false;
        }
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE)
        {
            ERROR("clearAllSessions - 删除会话信息失败: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        return true;
    }

    // ---------------------------消息---------------------------
    bool DataManager::insertMessage(const std::string& sessionId, const Message& message, std::time_t timestamp)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string sql = R"(
           INSERT INTO messages (message_id,session_id,role,content,timestamp)
           VALUES (?,?,?,?,?)
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(),-1,&stmt,nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about insertMessage: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt,1, message._message_id.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt,2, sessionId.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt,3, message._role.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt,4, message._content.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt,5, static_cast<sqlite3_int64>(timestamp));
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE)
        {
            ERROR("Failed to insert message: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        std::shared_ptr<Session> session = GetSessionByIdNoLock(sessionId);
        bool ret = updateSessionStateNoLock(session, timestamp);
        return ret;
    }

    std::vector<Message> DataManager::GetMessagesBySessionId(const std::string& sessionId)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string sql = R"(
            SELECT * FROM messages WHERE session_id = ?
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about GetMessagesBySessionId: {}", sqlite3_errmsg(_db));
            return {};
        }
        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        std::vector<Message> messages;
        messages.reserve(32); // 预分配空间，避免多次扩容
        while(sqlite3_step(stmt) == SQLITE_ROW)
        {
            Message msg( reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
            msg._message_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            messages.push_back(msg);
        }
        sqlite3_finalize(stmt);
        return messages;
    }

    // 删除会话所有的消息
    bool DataManager::deleteSessionMessage(const std::string& sessionId)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string sql = R"(
            DELETE FROM messages WHERE session_id = ?
        )";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if(rc != SQLITE_OK)
        {
            ERROR("Failed to prepare statement about deleteSessionMessage: {}", sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if(rc != SQLITE_DONE)
        {
            ERROR("Failed to delete session message: {}", sqlite3_errmsg(_db));
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        return true;
    }
}
