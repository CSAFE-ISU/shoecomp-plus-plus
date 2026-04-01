// worker_channel.hpp
// Thread-safe channel between a worker thread and an ImGui render loop.
// Worker writes. Render loop reads. No blocking on either side.

#ifndef ALIGNCALC_WORKERCHANNEL_H
#define ALIGNCALC_WORKERCHANNEL_H
#include <atomic>
#include <array>
#include <string>
#include <optional>
#include <cstring>

enum class MsgKind : uint8_t
{
    Log,
    Warning,
    Error,
    Progress,
    Done,
    Cancelled
};

struct WorkerMsg
{
    MsgKind kind;
    float progress;  // 0.0–1.0, valid when kind == Progress
    char text[120];  // fixed size keeps the struct trivially copyable
};

// Lock-free SPSC ring buffer
// Single Producer (worker), Single Consumer (render loop).

template <size_t N>  // N must be power of 2
class SPSCRing
{
    static_assert((N & (N - 1)) == 0, "N must be power of 2");

    std::array<WorkerMsg, N> buf_;
    std::atomic<size_t> head_{0};  // writer advances
    std::atomic<size_t> tail_{0};  // reader advances

   public:
    // Called from worker thread only.
    bool push(const WorkerMsg& msg)
    {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t next = (h + 1) & (N - 1);
        if (next == tail_.load(std::memory_order_acquire))
            return false;  // full — caller can drop or retry
        buf_[h] = msg;
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Called from render loop only.
    std::optional<WorkerMsg> pop()
    {
        size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire))
            return std::nullopt;  // empty
        WorkerMsg msg = buf_[t];
        tail_.store((t + 1) & (N - 1), std::memory_order_release);
        return msg;
    }
};

// The single shared object. Both threads hold a pointer to it.
struct WorkerChannel
{
    std::atomic<bool> cancel_requested{false};
    std::atomic<bool> is_running{false};
    std::atomic<float> progress{0.0f};
    SPSCRing<64> messages;

    void post(MsgKind kind, const char* text = "", float p = 0.0f)
    {
        WorkerMsg m{};
        m.kind = kind;
        m.progress = p;
        std::strncpy(m.text, text, sizeof(m.text) - 1);
        messages.push(m);
    }

    void log(const char* t) { post(MsgKind::Log, t); }
    void warn(const char* t) { post(MsgKind::Warning, t); }
    void error(const char* t) { post(MsgKind::Error, t); }
    void report(float p, const char* t)
    {
        progress.store(p);
        post(MsgKind::Progress, t, p);
    }
    void done()
    {
        post(MsgKind::Done);
        is_running.store(false);
    }
    void cancelled()
    {
        post(MsgKind::Cancelled);
        is_running.store(false);
    }

    bool should_cancel() const { return cancel_requested.load(); }
};
#endif
