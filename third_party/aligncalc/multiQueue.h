#ifndef ALIGNCALC_MULTIQUEUE_H
#define ALIGNCALC_MULTIQUEUE_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <thread>

namespace AlignCalc
{
    static constexpr size_t kCacheLine = 64;

    // ------------------------------------------------------------------
    // Slot
    //
    // sequence encodes slot state relative to a generation
    // counter:
    //
    //   sequence == pos        → slot is free, producer may write
    //   sequence == pos + 1    → slot is published, consumer may
    //   claim sequence == pos + Cap  → slot is consumed, producer
    //   may reuse
    //                             (this is the same as seq ==
    //                             next_pos
    //                              because pos wraps modulo Cap)
    //
    // The producer only advances write_pos after publishing.
    // Consumers race via CAS on read_pos; winner marks sequence.
    // ------------------------------------------------------------------
    template<typename T>
    struct Slot
    {
        T data{};
        std::atomic<size_t> sequence{0};

        Slot() noexcept = default;
        Slot(const Slot&) = delete;
        Slot& operator=(const Slot&) = delete;
    };

    class Backoff
    {
       public:
        static constexpr int kMaxSleepUs = 10485760U;

        void wait()
        {
            int us = 1 << count;
            if (us > kMaxSleepUs) us = kMaxSleepUs;
            std::this_thread::sleep_for(std::chrono::microseconds(us));
            ++count;
            if (count > 30) count = 30;
        }

        void reset() noexcept { count = 0; }

       private:
        int count = 0;
    };

    // ---------------------------------------------------------------------------
    // SPMCQueue<T, Cap>
    //
    // Single-producer, multiple-consumer bounded queue.
    // - One producer: calls push() / stop()
    // - N consumers:  call pop()
    //
    // Guarantees:
    //   - Each item delivered to exactly one consumer (work-stealing,
    //   not broadcast)
    //   - Drain stop: stop() signals shutdown; consumers finish
    //   draining the queue
    //     before returning false
    //   - Producer blocks (with backoff) when the queue is full
    //   - push() returns false only after stop() has been called
    //   - pop() returns false only when stopped AND queue is empty
    //
    // Cap must be a power of two.
    // T must be default-constructible and copy/move-assignable.
    // ---------------------------------------------------------------------------
    template <typename T, size_t Cap>
    class SPMCQueue
    {
        static_assert((Cap & (Cap - 1)) == 0,
                      "Cap must be a power of two");
        static_assert(Cap >= 2, "Cap must be at least 2");
        static_assert(std::is_default_constructible_v<T>);

        alignas(kCacheLine) Slot<T> buffer[Cap];
        alignas(kCacheLine) std::atomic<size_t> write_pos{0};
        alignas(kCacheLine) std::atomic<size_t> read_pos{0};
        alignas(kCacheLine) std::atomic<bool> running{true};

       public:
        SPMCQueue() noexcept
        {
            // Initialise sequence numbers:
            // slot i is free at generation 0 when sequence == i.
            for (size_t i = 0; i < Cap; ++i)
                buffer[i].sequence.store(i, std::memory_order_relaxed);
        }

        ~SPMCQueue() = default;

        // Non-copyable, non-movable (atomics + alignment)
        SPMCQueue(const SPMCQueue&) = delete;
        SPMCQueue& operator=(const SPMCQueue&) = delete;

        // ------------------------------------------------------------------
        // stop()
        //
        // Called by the producer to signal shutdown.
        // Consumers drain remaining items before returning false from
        // pop(). push() returns false immediately after this is called.
        // ------------------------------------------------------------------
        void stop() noexcept
        {
            running.store(false, std::memory_order_release);
        }

        bool is_running() const noexcept
        {
            return running.load(std::memory_order_acquire);
        }

        // ------------------------------------------------------------------
        // push(val)
        //
        // Blocks with exponential backoff when the queue is full.
        // Returns true on success, false if stop() was called while
        // waiting.
        //
        // Only ONE thread may call push(). Concurrent pushes are
        // undefined.
        // ------------------------------------------------------------------
        bool push(const T& val) { return push_impl(val); }
        bool push(T&& val) { return push_impl(std::move(val)); }

        // ------------------------------------------------------------------
        // try_push(val)
        //
        // Non-blocking: attempts a single push and returns immediately.
        // Returns true on success.
        // Returns false if the queue is full OR stop() has been called.
        //
        // Use this when the producer needs to interleave pushing work
        // with other work (e.g. draining a result queue) without
        // committing to a blocking wait. Pair with a caller-side retry
        // loop.
        //
        // Only ONE thread may call try_push(). Concurrent calls are
        // undefined.
        // ------------------------------------------------------------------
        bool try_push(const T& val) { return try_push_impl(val); }
        bool try_push(T&& val) { return try_push_impl(std::move(val)); }

        // ------------------------------------------------------------------
        // pop(out)
        //
        // Blocks with exponential backoff when the queue is empty.
        // Returns true and writes to out on success.
        // Returns false when stopped AND the queue is fully drained.
        //
        // Multiple threads may call pop() concurrently.
        // ------------------------------------------------------------------
        bool pop(T& out)
        {
            Backoff bo;

            while (true)
            {
                size_t pos = read_pos.load(std::memory_order_relaxed);

                // Drain check: if stopped and nothing left to read,
                // exit. We check write_pos with acquire to see all
                // published stores.
                if (!running.load(std::memory_order_acquire) &&
                    pos >= write_pos.load(std::memory_order_acquire))
                {
                    return false;
                }

                auto& slot = buffer[pos & (Cap - 1)];
                size_t seq =
                    slot.sequence.load(std::memory_order_acquire);

                if (seq == pos + 1)
                {
                    // Slot is published. Race to claim it.
                    if (read_pos.compare_exchange_weak(
                            pos, pos + 1, std::memory_order_acq_rel,
                            std::memory_order_relaxed))
                    {
                        // We won. Extract data and release slot back to
                        // producer.
                        out = std::move(slot.data);
                        slot.sequence.store(pos + Cap,
                                            std::memory_order_release);
                        bo.reset();
                        return true;
                    }
                    // Lost the CAS: another consumer claimed this slot.
                    // pos was updated by CWE failure. Retry without
                    // backoff — there may be another slot ready
                    // immediately.
                }
                else
                {
                    // Nothing ready yet. Back off.
                    bo.wait();
                }
            }
        }

        static constexpr size_t capacity() noexcept { return Cap; }

       private:
        template <typename U>
        bool try_push_impl(U&& val)
        {
            if (!running.load(std::memory_order_acquire)) return false;

            size_t pos = write_pos.load(std::memory_order_relaxed);
            auto& slot = buffer[pos & (Cap - 1)];

            if (slot.sequence.load(std::memory_order_acquire) != pos)
                return false;  // full

            slot.data = std::forward<U>(val);
            slot.sequence.store(pos + 1, std::memory_order_release);
            write_pos.store(pos + 1, std::memory_order_relaxed);
            return true;
        }

        template <typename U>
        bool push_impl(U&& val)
        {
            Backoff bo;

            while (true)
            {
                if (!running.load(std::memory_order_acquire))
                    return false;

                size_t pos = write_pos.load(std::memory_order_relaxed);
                auto& slot = buffer[pos & (Cap - 1)];
                size_t seq =
                    slot.sequence.load(std::memory_order_acquire);

                if (seq == pos)
                {
                    // Slot is free. Write and publish.
                    slot.data = std::forward<U>(val);
                    slot.sequence.store(pos + 1,
                                        std::memory_order_release);
                    write_pos.store(pos + 1, std::memory_order_relaxed);
                    return true;
                }

                // seq == pos + Cap would mean we lapped a consumer —
                // queue full. Any other value is a transient state;
                // back off either way.
                bo.wait();
            }
        }
    };

    // ---------------------------------------------------------------------------
    // MPSCQueue<T, Cap>
    //
    // Multiple-producer, single-consumer bounded queue.
    // Structural inverse of SPMCQueue:
    //
    //   SPMCQueue: write_pos exclusive (producer), read_pos shared CAS
    //   (consumers) MPSCQueue: write_pos shared CAS (producers),
    //   read_pos exclusive (consumer)
    //
    // Intended as the result queue paired with SPMCQueue:
    //
    //   SPMCQueue<Task,   WorkCap>    work_q;    // producer →
    //   consumers MPSCQueue<Result, ResultCap>  result_q;  // consumers
    //   → producer (merger)
    //
    // Guarantees:
    //   - Each item delivered to the single consumer exactly once
    //   - Producers block (with exponential backoff) when the queue is
    //   full
    //   - push() returns false only after stop() has been called
    //   - pop() returns false when stopped AND queue is fully drained
    //   - Single consumer: pop() must not be called concurrently
    //
    // Cap must be a power of two.
    // T must be default-constructible and copy/move-assignable.
    //
    // Slot state machine (mirrors SPMCQueue exactly):
    //
    //   sequence == pos        → free, a producer may claim via CAS
    //   sequence == pos + 1    → published, consumer may read
    //   sequence == pos + Cap  → consumed, producers may reuse
    // ---------------------------------------------------------------------------
    template <typename T, size_t Cap>
    class MPSCQueue
    {
        static_assert((Cap & (Cap - 1)) == 0,
                      "Cap must be a power of two");
        static_assert(Cap >= 2, "Cap must be at least 2");
        static_assert(std::is_default_constructible_v<T>);

        alignas(kCacheLine) Slot<T> buffer[Cap];

        // Shared: multiple producers race via CAS.
        alignas(kCacheLine) std::atomic<size_t> write_pos{0};

        // Exclusive: only the single consumer touches this.
        // Plain size_t — no atomic needed.
        alignas(kCacheLine) size_t read_pos{0};

        // Written by producers (any), read by consumer.
        alignas(kCacheLine) std::atomic<bool> running{true};

       public:
        MPSCQueue() noexcept
        {
            for (size_t i = 0; i < Cap; ++i)
                buffer[i].sequence.store(i, std::memory_order_relaxed);
        }

        ~MPSCQueue() = default;

        MPSCQueue(const MPSCQueue&) = delete;
        MPSCQueue& operator=(const MPSCQueue&) = delete;

        // ------------------------------------------------------------------
        // stop()
        //
        // Any producer may call this. Signals the consumer to drain and
        // exit. Remaining items in the queue are still delivered before
        // pop() returns false (drain stop, not hard stop).
        // ------------------------------------------------------------------
        void stop() noexcept
        {
            running.store(false, std::memory_order_release);
        }

        bool is_running() const noexcept
        {
            return running.load(std::memory_order_acquire);
        }

        // ------------------------------------------------------------------
        // push(val)
        //
        // Called by producers (multiple threads concurrently).
        //
        // Protocol (inverted from SPMCQueue push):
        //   1. CAS write_pos to claim the slot  ← must happen before
        //   writing data
        //   2. Write data into the claimed slot
        //   3. Publish via sequence store
        //
        // Step 1 must precede step 2 because multiple threads compete
        // for the same slot. In SPMCQueue the producer owns write_pos
        // exclusively so no CAS was needed; here it is the contention
        // point.
        //
        // Blocks with exponential backoff when full.
        // Returns false if stop() was called while waiting.
        // ------------------------------------------------------------------
        bool push(const T& val) { return push_impl(val); }
        bool push(T&& val) { return push_impl(std::move(val)); }

        // ------------------------------------------------------------------
        // pop(out)
        //
        // Called by the SINGLE consumer only. Not thread-safe for
        // concurrent pop() calls.
        //
        // Non-blocking: returns false immediately if nothing is ready
        // OR if stopped and drained. The consumer is expected to loop
        // with its own backoff or merge logic.
        //
        // Drain behaviour: after stop(), pop() continues returning true
        // until all published items are consumed. Returns false only
        // when both:
        //   - running is false, AND
        //   - no published slot is waiting at read_pos_
        // ------------------------------------------------------------------
        bool pop(T& out)
        {
            auto& slot = buffer[read_pos & (Cap - 1)];
            size_t seq = slot.sequence.load(std::memory_order_acquire);

            if (seq != read_pos + 1)
            {
                // Nothing published at this position yet.
                // Check drain condition: stopped and write_pos has not
                // advanced past read_pos means queue is empty.
                if (!running.load(std::memory_order_acquire) &&
                    read_pos >=
                        write_pos.load(std::memory_order_acquire))
                    return false;

                return false;  // not ready, caller should back off and
                               // retry
            }

            out = std::move(slot.data);
            slot.sequence.store(read_pos + Cap,
                                std::memory_order_release);
            ++read_pos;
            return true;
        }

        // ------------------------------------------------------------------
        // pop_wait(out)
        //
        // Blocking variant for the consumer. Loops with exponential
        // backoff until an item is available or the queue is stopped
        // and drained.
        // ------------------------------------------------------------------
        bool pop_wait(T& out)
        {
            Backoff bo;
            while (true)
            {
                if (pop(out)) return true;

                // Recheck drain condition after failed pop.
                if (!running.load(std::memory_order_acquire) &&
                    read_pos >=
                        write_pos.load(std::memory_order_acquire))
                    return false;

                bo.wait();
            }
        }

        // ------------------------------------------------------------------
        // Approximate occupancy. Not exact under concurrency.
        // ------------------------------------------------------------------
        size_t size_approx() const noexcept
        {
            size_t w = write_pos.load(std::memory_order_relaxed);
            size_t r =
                read_pos;  // consumer-side read, no atomic needed
            return (w > r) ? (w - r) : 0;
        }

        static constexpr size_t capacity() noexcept { return Cap; }

       private:
        template <typename U>
        bool push_impl(U&& val)
        {
            Backoff bo;

            while (true)
            {
                if (!running.load(std::memory_order_acquire))
                    return false;

                size_t pos = write_pos.load(std::memory_order_relaxed);
                auto& slot = buffer[pos & (Cap - 1)];
                size_t seq =
                    slot.sequence.load(std::memory_order_acquire);

                if (seq == pos)
                {
                    // Slot appears free. Race to claim it via CAS.
                    // On success: we own the slot, write data, then
                    // publish. On failure: another producer claimed it;
                    // reload pos and retry.
                    if (write_pos.compare_exchange_weak(
                            pos, pos + 1, std::memory_order_acq_rel,
                            std::memory_order_relaxed))
                    {
                        slot.data = std::forward<U>(val);
                        slot.sequence.store(pos + 1,
                                            std::memory_order_release);
                        return true;
                    }
                    // CAS failed — retry without backoff, another slot
                    // may be free.
                }
                else
                {
                    // Queue full or transient state. Back off.
                    bo.wait();
                }
            }
        }
    };

}  // namespace AlignCalc

#endif
