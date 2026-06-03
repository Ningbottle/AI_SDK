/* ============================================================
   AI 聊天助手 — 主逻辑
   ============================================================ */

// ============================================================
// 1. 状态
// ============================================================
const state = {
    sessions: [], // 会话列表
    models: [], // 可用模型列表
    currentSessionId: null, // 当前选中的会话 ID
    streaming: false, // 是否正在流式接收
    streamingText: "", // 流式过程中累积的原始文本
    streamingBubble: null, // 流式消息的 DOM 气泡引用
    abortController: null, // 用于中断流式请求
};

// ============================================================
// 2. DOM 引用
// ============================================================
const $ = (id) => document.getElementById(id);
const sessionListEl = $("sessionList");
const messagesContainer = $("messagesContainer");
const welcomeScreen = $("welcomeScreen");
const inputArea = $("inputArea");
const messageInput = $("messageInput");
const sendBtn = $("sendBtn");
const wordCount = $("wordCount");
const modelModal = $("modelModal");
const modelGrid = $("modelGrid");
const modalConfirmBtn = $("modalConfirmBtn");
const modalCancelBtn = $("modalCancelBtn");
const confirmOverlay = $("confirmOverlay");
const confirmDeleteBtn = $("confirmDeleteBtn");
const confirmCancelBtn = $("confirmCancelBtn");

// ============================================================
// 3. 工具函数
// ============================================================

/** 格式化时间戳为相对时间 */
function formatTime(ts) {
    const now = Date.now() / 1000;
    const diff = now - ts;
    if (diff < 60) return "刚刚";
    if (diff < 3600) return `${Math.floor(diff / 60)} 分钟前`;
    if (diff < 86400) return `${Math.floor(diff / 3600)} 小时前`;
    if (diff < 2592000) return `${Math.floor(diff / 86400)} 天前`;
    const d = new Date(ts * 1000);
    return `${d.getMonth() + 1}/${d.getDate()}`;
}

/** 获取当前时间的 ISO 格式 */
function nowISO() {
    return new Date().toISOString().slice(0, 19).replace("T", " ");
}

/** 截断文本 */
function truncate(str, len = 40) {
    if (!str) return "新会话";
    return str.length > len ? str.slice(0, len) + "..." : str;
}

/** 转义 HTML */
function escapeHtml(text) {
    const div = document.createElement("div");
    div.textContent = text;
    return div.innerHTML;
}

/** 显示错误提示 */
function showToast(msg) {
    const old = document.querySelector(".toast");
    if (old) old.remove();
    const el = document.createElement("div");
    el.className = "toast";
    el.textContent = msg;
    document.body.appendChild(el);
    setTimeout(() => el.remove(), 3000);
}

// ============================================================
// 4. API 请求
// ============================================================

const API_BASE = "";

async function apiGet(path) {
    const res = await fetch(API_BASE + path);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
}

async function apiPost(path, body) {
    const res = await fetch(API_BASE + path, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(body),
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
}

async function apiDelete(path) {
    const res = await fetch(API_BASE + path, { method: "DELETE" });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
}

// ============================================================
// 5. 会话列表
// ============================================================

async function loadSessions() {
    try {
        const resp = await apiGet("/api/sessions");
        if (resp.success) {
            state.sessions = resp.data || [];
            renderSessions();
        }
    } catch (e) {
        console.error("加载会话列表失败:", e);
    }
}

function renderSessions() {
    if (!state.sessions.length) {
        sessionListEl.innerHTML = `<div class="empty-state">暂无历史对话</div>`;
        return;
    }

    sessionListEl.innerHTML = state.sessions
        .map((s) => {
            const active = s.id === state.currentSessionId ? " active" : "";
            return `
      <div class="session-item${active}" data-id="${s.id}">
        <div class="session-item-header">
          <span class="session-model">${escapeHtml(s.model || "")}</span>
          <span class="session-time">${formatTime(s.updated_at || s.created_at)}</span>
        </div>
        <div class="session-preview">${escapeHtml(truncate(s.first_user_message))}</div>
        <div class="session-delete-area">
          <button class="btn-danger" data-action="delete" data-id="${s.id}">...</button>
        </div>
      </div>
    `;
        })
        .join("");

    // 点击会话
    sessionListEl.querySelectorAll(".session-item").forEach((el) => {
        el.addEventListener("click", (e) => {
            if (e.target.closest('[data-action="delete"]')) return;
            const id = el.dataset.id;
            switchSession(id);
        });
    });

    // 删除按钮
    sessionListEl.querySelectorAll('[data-action="delete"]').forEach((btn) => {
        btn.addEventListener("click", (e) => {
            e.stopPropagation();
            const id = btn.dataset.id;
            showDeleteConfirm(id);
        });
    });
}

// ============================================================
// 6. 切换会话
// ============================================================

function switchSession(sessionId) {
    if (state.streaming) {
        if (state.abortController) state.abortController.abort();
        state.streaming = false;
    }

    state.currentSessionId = sessionId;

    // 更新侧边栏高亮
    document.querySelectorAll(".session-item").forEach((el) => {
        el.classList.toggle("active", el.dataset.id === sessionId);
    });

    // 切换视图
    welcomeScreen.style.display = "none";
    messagesContainer.style.display = "flex";
    inputArea.style.display = "block";
    messagesContainer.innerHTML = "";

    // 加载历史消息
    loadMessages(sessionId);
}

async function loadMessages(sessionId) {
    try {
        const resp = await apiGet(`/api/session/${sessionId}/history`);
        if (resp.success && resp.data) {
            resp.data.forEach((msg) =>
                appendMessage(msg.role, msg.content, msg.timestamp),
            );
        }
        scrollToBottom();
    } catch (e) {
        console.error("加载消息失败:", e);
        showToast("加载消息失败");
    }
}

// ============================================================
// 7. 消息渲染
// ============================================================

function appendMessage(role, content, timestamp) {
    const div = document.createElement("div");
    div.className = `message message-${role === "user" ? "user" : "ai"}`;

    const bubble = document.createElement("div");
    bubble.className = "message-bubble";

    if (role === "assistant") {
        bubble.innerHTML = renderMarkdown(content);
        // 代码高亮
        bubble.querySelectorAll("pre code").forEach((block) => {
            hljs.highlightElement(block);
            addCopyButton(block.parentElement);
        });
    } else {
        bubble.textContent = content;
    }

    div.appendChild(bubble);

    const ts = document.createElement("div");
    ts.className = "message-timestamp";
    ts.textContent = timestamp ? formatTime(timestamp) : nowISO();
    div.appendChild(ts);

    messagesContainer.appendChild(div);
    scrollToBottom();
}

function appendStreamingChunk(text) {
    state.streamingText += text;

    // 流式过程中只显示纯文本，不渲染 markdown
    if (state.streamingBubble) {
        state.streamingBubble.textContent = state.streamingText;
    } else {
        // 创建 AI 消息气泡，插到 typing 之前
        const container = messagesContainer;
        const div = document.createElement("div");
        div.className = "message message-ai";
        const bubble = document.createElement("div");
        bubble.className = "message-bubble";
        bubble.textContent = state.streamingText;
        div.appendChild(bubble);
        const ts = document.createElement("div");
        ts.className = "message-timestamp";
        div.appendChild(ts);

        const typing = container.querySelector(".typing-indicator");
        if (typing) {
            container.insertBefore(div, typing.closest(".message-ai"));
        } else {
            container.appendChild(div);
        }
        state.streamingBubble = bubble;
    }
    scrollToBottom();
}

function removeTransientAiBubbles() {
    messagesContainer
        .querySelectorAll(".typing-indicator")
        .forEach((typing) => {
            const outer = typing.closest(".message-ai");
            if (outer) outer.remove();
        });

    messagesContainer.querySelectorAll(".message-ai").forEach((msg) => {
        if (state.streamingBubble && msg.contains(state.streamingBubble))
            return;

        const bubble = msg.querySelector(".message-bubble");
        const hasTyping = !!msg.querySelector(".typing-indicator");
        const hasContent = bubble && bubble.textContent.trim().length > 0;
        if (hasTyping || !hasContent) msg.remove();
    });
}

function finalizeStreaming() {
    removeTransientAiBubbles();

    const finalText = state.streamingText;
    console.log(
        "[DEBUG-finalize] streamingText length:",
        finalText.length,
        "first 200 chars:",
        finalText.slice(0, 200),
    );
    if (state.streamingBubble && finalText.trim()) {
        state.streamingBubble.innerHTML = renderMarkdown(finalText);
        state.streamingBubble.querySelectorAll("pre code").forEach((block) => {
            hljs.highlightElement(block);
            addCopyButton(block.parentElement);
        });

        const msgDiv = state.streamingBubble.closest(".message-ai");
        if (msgDiv) {
            const ts = msgDiv.querySelector(".message-timestamp");
            if (ts) ts.textContent = nowISO();
        }
    } else if (state.streamingBubble) {
        const msgDiv = state.streamingBubble.closest(".message-ai");
        if (msgDiv) msgDiv.remove();
    }

    removeTransientAiBubbles();

    // 清理状态
    state.streamingText = "";
    state.streamingBubble = null;
}

function showTyping() {
    const div = document.createElement("div");
    div.className = "message message-ai";
    div.innerHTML = `
    <div class="message-bubble">
      <div class="typing-indicator">
        <span></span><span></span><span></span>
      </div>
    </div>
  `;
    messagesContainer.appendChild(div);
    scrollToBottom();
}

function scrollToBottom() {
    setTimeout(() => {
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }, 10);
}

// ============================================================
// 8. Markdown 渲染
// ============================================================

function renderMarkdown(text) {
    if (!text) return "";
    try {
        return marked.parse(text, {
            breaks: true,
            gfm: true,
        });
    } catch {
        return escapeHtml(text);
    }
}

async function copyText(text) {
    if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(text);
        return;
    }

    const textarea = document.createElement("textarea");
    textarea.value = text;
    textarea.setAttribute("readonly", "");
    textarea.style.position = "fixed";
    textarea.style.left = "-9999px";
    textarea.style.top = "0";
    document.body.appendChild(textarea);
    textarea.focus();
    textarea.select();

    const ok = document.execCommand("copy");
    textarea.remove();
    if (!ok) throw new Error("execCommand copy failed");
}

/** 给代码块添加复制按钮 */
function addCopyButton(preEl) {
    if (preEl.querySelector(".copy-btn")) return;
    const btn = document.createElement("button");
    btn.className = "copy-btn";
    btn.textContent = "复制";
    btn.addEventListener("click", async () => {
        const code = preEl.querySelector("code");
        if (!code) return;

        try {
            await copyText(code.textContent);
            btn.textContent = "已复制";
            btn.classList.add("copied");
            setTimeout(() => {
                btn.textContent = "复制";
                btn.classList.remove("copied");
            }, 2000);
        } catch (e) {
            console.error("复制失败:", e);
            showToast("复制失败，请手动选择复制");
        }
    });
    preEl.appendChild(btn);
}

// ============================================================
// 9. 发送消息（流式）
// ============================================================

async function sendMessage() {
    const text = messageInput.value.trim();
    if (!text || state.streaming || !state.currentSessionId) return;

    state.streamingText = "";
    state.streamingBubble = null;

    // 清空输入
    messageInput.value = "";
    updateWordCount();
    sendBtn.disabled = true;

    // 追加用户消息
    appendMessage("user", text);

    // 显示 typing
    showTyping();

    state.streaming = true;
    state.abortController = new AbortController();

    try {
        const res = await fetch(API_BASE + "/api/message/async", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
                session_id: state.currentSessionId,
                message: text,
            }),
            signal: state.abortController.signal,
        });

        if (!res.ok) {
            showToast(`服务器错误: ${res.status}`);
            finalizeStreaming();
            state.streaming = false;
            return;
        }

        const reader = res.body.getReader();
        const decoder = new TextDecoder();
        let buffer = "";
        let streamDone = false;

        const processSseEvent = (eventText) => {
            const dataLines = [];
            for (const rawLine of eventText.split("\n")) {
                const line = rawLine.endsWith("\r")
                    ? rawLine.slice(0, -1)
                    : rawLine;
                if (line.startsWith("data:")) {
                    dataLines.push(
                        line.startsWith("data: ")
                            ? line.slice(6)
                            : line.slice(5),
                    );
                }
            }

            if (!dataLines.length) return;

            const payload = dataLines.join("\n");
            if (payload === "[DONE]") {
                streamDone = true;
                finalizeStreaming();
                return;
            }

            try {
                const event = JSON.parse(payload);
                if (event.done) {
                    streamDone = true;
                    finalizeStreaming();
                    return;
                }
                if (typeof event.content === "string") {
                    appendStreamingChunk(event.content);
                }
            } catch (e) {
                // 兼容旧服务端的纯文本 SSE 格式。
                appendStreamingChunk(payload);
            }
        };

        while (true) {
            const { done, value } = await reader.read();
            if (done) break;

            buffer += decoder.decode(value, { stream: true });
            const events = buffer.split(/\r?\n\r?\n/);
            buffer = events.pop() || "";

            for (const eventText of events) {
                processSseEvent(eventText);
            }
        }

        buffer += decoder.decode();
        if (buffer) {
            processSseEvent(buffer);
        }

        if (!streamDone) {
            finalizeStreaming();
        }
    } catch (e) {
        if (e.name === "AbortError") {
            finalizeStreaming();
        } else {
            console.error("发送消息失败:", e);
            showToast("发送消息失败");
            finalizeStreaming();
        }
    }

    state.streaming = false;
    state.abortController = null;
    updateSendBtn();
}

// ============================================================
// 10. 新建对话
// ============================================================

async function openNewChatModal() {
    try {
        const resp = await apiGet("/api/models");
        if (!resp.success) {
            showToast("获取模型列表失败");
            return;
        }
        state.models = resp.data || [];
        renderModelGrid();
        modelModal.classList.add("open");
    } catch (e) {
        console.error("获取模型列表失败:", e);
        showToast("获取模型列表失败");
    }
}

function renderModelGrid() {
    if (!state.models.length) {
        modelGrid.innerHTML =
            '<div class="empty-state" style="grid-column:1/-1">暂无可用模型</div>';
        return;
    }

    modelGrid.innerHTML = state.models
        .map(
            (m, i) => `
    <div class="model-option">
      <input type="radio" name="model" id="model_${i}" value="${escapeHtml(m.name)}" ${i === 0 ? "checked" : ""}>
      <label for="model_${i}">
        <div class="model-option-name">${escapeHtml(m.name)}</div>
        <div class="model-option-desc">${escapeHtml(m.desc || "")}</div>
      </label>
    </div>
  `,
        )
        .join("");
}

async function confirmNewSession() {
    const selected = document.querySelector('input[name="model"]:checked');
    if (!selected) {
        showToast("请选择一个模型");
        return;
    }

    const modelName = selected.value;
    modelModal.classList.remove("open");

    try {
        const resp = await apiPost("/api/session", { model: modelName });
        if (!resp.success || !resp.data) {
            showToast(resp.message || "创建会话失败");
            return;
        }

        const session = resp.data;
        // 将新会话加入列表
        const newSession = {
            id: session.session_id,
            model: session.model,
            created_at: Math.floor(Date.now() / 1000),
            updated_at: Math.floor(Date.now() / 1000),
            message_count: 0,
            first_user_message: "",
        };

        state.sessions.unshift(newSession);
        renderSessions();

        // 切换到新会话
        switchSession(session.session_id);
    } catch (e) {
        console.error("创建会话失败:", e);
        showToast("创建会话失败");
    }
}

// ============================================================
// 11. 删除会话
// ============================================================

let pendingDeleteId = null;

function showDeleteConfirm(sessionId) {
    pendingDeleteId = sessionId;
    confirmOverlay.classList.add("open");
}

async function confirmDelete() {
    if (!pendingDeleteId) return;
    const id = pendingDeleteId;
    pendingDeleteId = null;
    confirmOverlay.classList.remove("open");

    try {
        const resp = await apiDelete(`/api/session/${id}`);
        if (!resp.success) {
            showToast(resp.message || "删除失败");
            return;
        }

        // 从列表中移除
        state.sessions = state.sessions.filter((s) => s.id !== id);
        renderSessions();

        // 如果是当前会话，回到欢迎页
        if (state.currentSessionId === id) {
            state.currentSessionId = null;
            welcomeScreen.style.display = "flex";
            messagesContainer.style.display = "none";
            inputArea.style.display = "none";
            messagesContainer.innerHTML = "";
        }
    } catch (e) {
        console.error("删除会话失败:", e);
        showToast("删除会话失败");
    }
}

// ============================================================
// 12. 输入框控制
// ============================================================

const MAX_LEN = 2000;

function updateWordCount() {
    const len = messageInput.value.length;
    wordCount.textContent = `${len}/${MAX_LEN}`;
    wordCount.classList.toggle("over-limit", len > MAX_LEN);
}

function updateSendBtn() {
    const hasText = messageInput.value.trim().length > 0;
    sendBtn.disabled = !hasText || state.streaming || !state.currentSessionId;
}

// 自动调整 textarea 高度
function autoResize() {
    messageInput.style.height = "auto";
    messageInput.style.height = Math.min(messageInput.scrollHeight, 160) + "px";
}

// ============================================================
// 13. 事件绑定
// ============================================================

// --- 新建对话 ---
document.querySelectorAll("#newChatBtn, #welcomeNewBtn").forEach((btn) => {
    btn.addEventListener("click", openNewChatModal);
});

// --- 侧边栏切换（移动端） ---
const sidebarToggle = document.getElementById("sidebarToggle");
const sidebar = document.querySelector(".sidebar");
if (sidebarToggle && sidebar) {
    // 创建遮罩层
    const backdrop = document.createElement("div");
    backdrop.className = "sidebar-backdrop";
    document.body.appendChild(backdrop);

    sidebarToggle.addEventListener("click", () => {
        sidebar.classList.toggle("open");
        backdrop.classList.toggle("open");
    });

    backdrop.addEventListener("click", () => {
        sidebar.classList.remove("open");
        backdrop.classList.remove("open");
    });

    // 点击会话后自动关闭侧边栏
    document.addEventListener("click", (e) => {
        const item = e.target.closest(".session-item");
        if (item && window.innerWidth <= 768) {
            sidebar.classList.remove("open");
            backdrop.classList.remove("open");
        }
    });
}

// --- 模态框 ---
modalConfirmBtn.addEventListener("click", confirmNewSession);
modalCancelBtn.addEventListener("click", () => {
    modelModal.classList.remove("open");
});
modelModal.addEventListener("click", (e) => {
    if (e.target === modelModal) modelModal.classList.remove("open");
});

// --- 删除确认 ---
confirmDeleteBtn.addEventListener("click", confirmDelete);
confirmCancelBtn.addEventListener("click", () => {
    confirmOverlay.classList.remove("open");
    pendingDeleteId = null;
});
confirmOverlay.addEventListener("click", (e) => {
    if (e.target === confirmOverlay) {
        confirmOverlay.classList.remove("open");
        pendingDeleteId = null;
    }
});

// --- 输入框 ---
messageInput.addEventListener("input", () => {
    updateWordCount();
    updateSendBtn();
    autoResize();
});

messageInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter" && !e.shiftKey) {
        e.preventDefault();
        sendMessage();
    }
});

// --- 发送按钮 ---
sendBtn.addEventListener("click", sendMessage);

// ============================================================
// 14. 初始化
// ============================================================

loadSessions();
updateWordCount();
updateSendBtn();
