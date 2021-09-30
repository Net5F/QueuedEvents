#pragma once

#include "readerwriterqueue.h"
#include <vector>
#include <array>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <utility>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace AM
{
/**
 * This header contains Dispatcher and EventQueue, which together form a
 * queued event dispatching system.
 *
 * Features:
 *   Events may be any arbitrary type.
 *
 *   Notifications are thread-safe and queued.
 *
 * Thread safety:
 *   push(), emplace(), subscribe(), and unsubscribe() will block if any of the
 *   others are running. This means they're safe to call across threads,
 *   but may not be perfectly performant.
 *
 * Note: It would be ideal to have the classes in separate headers, but we
 *       would run into circular include issues.
 */

// Forward declarations.
template<typename T>
class EventQueue;
template<typename T>
class EventSorter;

//--------------------------------------------------------------------------
// EventDispatcher
//--------------------------------------------------------------------------
/**
 * A simple event dispatcher.
 *
 * Through push<T>() and emplace<T>(), dispatches events to any subscribed
 * EventQueues of a matching type T.
 *
 * Through push<T>(tickNum), dispatches events to any subscribed EventSorters
 * of a matching type T.
 */
class EventDispatcher
{
public:
    // See queueVectorMap comment for details.
    using QueueVector = std::vector<void*>;
    using SorterVector = std::vector<void*>;

    /**
     * Pushes the given event to all EventQueues of type T.
     */
    template<typename T>
    void push(const T& event)
    {
        // Acquire a lock before accessing a queue.
        std::scoped_lock lock(queueVectorMapMutex);

        // Get the vector of queues for type T.
        QueueVector& queueVector{getQueueVectorForType<T>()};

        // Push the given event into all queues.
        for (auto& queue : queueVector) {
            // Cast the queue to type T.
            EventQueue<T>* castQueue{static_cast<EventQueue<T>*>(queue)};

            // Push the event.
            castQueue->push(event);
        }
    }

    /**
     * Constructs the given event in place in all EventQueues of type T.
     */
    template<typename T, typename... Args>
    void emplace(Args&&... args)
    {
        // Acquire a lock before accessing a queue.
        std::scoped_lock lock(queueVectorMapMutex);

        // Get the vector of queues for type T.
        QueueVector& queueVector{getQueueVectorForType<T>()};

        // Push the given event into all queues.
        for (auto& queue : queueVector) {
            // Cast the queue to type T.
            EventQueue<T>* castQueue{static_cast<EventQueue<T>*>(queue)};

            // Push the event.
            castQueue->emplace(std::forward<Args>(args)...);
        }
    }

    /**
     * Pushes the given event to all EventSorters of type T.
     */
    template<typename T>
    void push(const T& event, uint32_t tickNum)
    {
        // Acquire a lock before accessing a sorter.
        std::scoped_lock lock(sorterVectorMapMutex);

        // Get the vector of sorters for type T.
        SorterVector& sorterVector{getSorterVectorForType<T>()};

        // Push the given event into all sorters.
        for (auto& sorter : sorterVector) {
            // Cast the sorter to type T.
            EventSorter<T>* castSorter{static_cast<EventSorter<T>*>(sorter)};

            // Push the event.
            castSorter->push(event, tickNum);
        }
    }

    /**
     * Returns the number of queues that are currently constructed for type T.
     *
     * Not thread safe. Really only useful for testing.
     */
    template<typename T>
    std::size_t getNumQueuesForType()
    {
        // Get the vector of queues for type T.
        QueueVector& queueVector{getQueueVectorForType<T>()};

        // Return the number of event queues.
        return queueVector.size();
    }

    /**
     * Same as getNumQueuesForType(), but for event sorters.
     */
    template<typename T>
    std::size_t getNumSortersForType()
    {
        // Get the vector of sorters for type T.
        SorterVector& sorterVector{getSorterVectorForType<T>()};

        // Return the number of event sorters.
        return sorterVector.size();
    }

private:
    /** Only give EventQueue and EventSorter access to subscribe() and
        unsubscribe(), so users don't get confused about how to use the
        interface. */
    template<typename>
    friend class EventQueue;
    template<typename>
    friend class EventSorter;

    /**
     * Returns the next unique integer key value.
     */
    static int getNextKey()
    {
        static std::atomic<int> nextKey{0};
        return nextKey++;
    }

    template<typename T>
    int getKeyForType()
    {
        // Get the key associated with type T.
        // Note: Static var means that getNextKey() will only be called once
        //       per type T, and that this key will be consistent across all
        //       Observer instances.
        static int key{getNextKey()};
        return key;
    }

    /**
     * Returns the vector of event queues that hold type T.
     */
    template<typename T>
    QueueVector& getQueueVectorForType()
    {
        // Return the vector at the given key.
        // (Constructs one if there is no existing vector.)
        return queueVectorMap[getKeyForType<T>()];
    }

    /**
     * Returns the vector of event sorters that hold type T.
     */
    template<typename T>
    SorterVector& getSorterVectorForType()
    {
        // Return the vector at the given key.
        // (Constructs one if there is no existing vector.)
        return sorterVectorMap[getKeyForType<T>()];
    }

    /**
     * Subscribes the given queue to receive event notifications.
     *
     * Only used by EventQueue.
     */
    template<typename T>
    void subscribe(EventQueue<T>* queuePtr)
    {
        // Acquire a lock since we're going to be modifying data structures.
        std::scoped_lock lock(queueVectorMapMutex);

        // Get the vector of queues for type T.
        QueueVector& queueVector{getQueueVectorForType<T>()};

        // Add the given queue to the vector.
        queueVector.push_back(static_cast<void*>(queuePtr));
    }

    /**
     * Unsubscribes the given event queue. It will no longer receive event
     * notifications.
     *
     * Only used by EventQueue.
     */
    template<typename T>
    void unsubscribe(const EventQueue<T>* unsubQueuePtr)
    {
        // Acquire a lock since we're going to be modifying data structures.
        std::scoped_lock lock(queueVectorMapMutex);

        // Get the vector of queues for type T.
        QueueVector& queueVector{getQueueVectorForType<T>()};

        // Search for the given queue in the vector.
        auto queueIt = queueVector.begin();
        for (; queueIt != queueVector.end(); ++queueIt) {
            if (*queueIt == unsubQueuePtr) {
                break;
            }
        }

        // Error if we didn't find the queue.
        // Note: Since this function is only called in EventQueue's
        //       destructor, we should expect it to always find the queue.
        if (queueIt == queueVector.end()) {
            std::cout << "Failed to find queue while unsubscribing."
                      << std::endl;
            std::abort();
        }

        // Remove the queue at the given index.
        // Note: This may do some extra work to fill the gap, but there's
        //       probably max 2-3 elements and pointers are small.
        queueVector.erase(queueIt);
    }

    /**
     * Override for EventSorters.
     */
    template<typename T>
    void subscribe(EventSorter<T>* sorterPtr)
    {
        // Acquire a lock since we're going to be modifying data structures.
        std::scoped_lock lock(sorterVectorMapMutex);

        // Get the vector of sorters for type T.
        SorterVector& sorterVector{getSorterVectorForType<T>()};

        // Add the given sorter to the vector.
        sorterVector.push_back(static_cast<void*>(sorterPtr));
    }

    /**
     * Override for EventSorters.
     */
    template<typename T>
    void unsubscribe(const EventSorter<T>* unsubSorterPtr)
    {
        // Acquire a lock since we're going to be modifying data structures.
        std::scoped_lock lock(sorterVectorMapMutex);

        // Get the vector of queues for type T.
        SorterVector& sorterVector{getSorterVectorForType<T>()};

        // Search for the given queue in the vector.
        auto sorterIt = sorterVector.begin();
        for (; sorterIt != sorterVector.end(); ++sorterIt) {
            if (*sorterIt == unsubSorterPtr) {
                break;
            }
        }

        // Error if we didn't find the sorter.
        // Note: Since this function is only called in EventSorter's
        //       destructor, we should expect it to always find the sorter.
        if (sorterIt == sorterVector.end()) {
            std::cout << "Failed to find sorter while unsubscribing."
                      << std::endl;
            std::abort();
        }

        // Remove the sorter at the given index.
        // Note: This may do some extra work to fill the gap, but there's
        //       probably max 2-3 elements and pointers are small.
        sorterVector.erase(sorterIt);
    }

    /** A map of integer keys -> vectors of event queues.
        We store each EventQueue<T> as a void pointer so that they can share
        a vector, then cast them to the appropriate type in our templated
        functions. */
    std::unordered_map<int, QueueVector> queueVectorMap;

    /** Used to lock access to the queueVectorMap. */
    std::mutex queueVectorMapMutex;

    /** Same as queueVectorMap, but for event sorters. */
    std::unordered_map<int, SorterVector> sorterVectorMap;

    /** Used to lock access to the sorterVectorMap. */
    std::mutex sorterVectorMapMutex;
};

//--------------------------------------------------------------------------
// EventQueue
//--------------------------------------------------------------------------
/**
 * A simple event listener queue.
 *
 * Supports pushing events into the queue (done by Dispatcher), and popping
 * events off the queue.
 */
template<typename T>
class EventQueue
{
public:
    EventQueue(EventDispatcher& inDispatcher)
    : dispatcher{inDispatcher}
    {
        dispatcher.subscribe(this);
    }

    ~EventQueue()
    {
        // Note: This acquires a write lock, don't destruct this queue unless
        //       you don't mind potentially waiting.
        dispatcher.unsubscribe<T>(this);
    }

    /**
     * Attempts to pop the front event from the queue.
     *
     * @return true if an event was available, else false.
     */
    bool pop(T& event)
    {
        // Try to pop an event.
        return queue.try_dequeue(event);
    }

    /**
     * Override that removes the front event of the queue without returning
     * it.
     *
     * @return true if an event was available, else false.
     */
    bool pop() { return queue.pop(); }

    /**
     * Attempts to pop the front event from the queue, blocking and waiting
     * for up to timeoutUs microseconds before returning false if one is not
     * available.
     *
     * A negative timeoutUs causes an indefinite wait.
     *
     * @return true if an event was available, else false if we timed out.
     */
    bool waitPop(T& event, std::int64_t timeoutUs)
    {
        return queue.wait_dequeue_timed(event, timeoutUs);
    }

    /**
     * @return If the queue is empty, returns nullptr. Else, returns a pointer
     * to the front event of the queue (the one that would be removed by the
     * next call to pop()).
     */
    T* peek() const { return queue.peek(); }

    /**
     * Returns the number of elements in the queue.
     */
    std::size_t size() { return queue.size_approx(); }

private:
    /** Only give EventDispatcher access to push(), so users don't get confused
        about how to use the interface. */
    friend class EventDispatcher;

    /**
     * Pushes the given event into the queue.
     *
     * Errors if a memory allocation fails while pushing the event into the
     * queue.
     */
    void push(const T& event)
    {
        // Push the event into the queue.
        if (!(queue.enqueue(event))) {
            std::cout << "Memory allocation failed while pushing an event."
                      << std::endl;
            std::abort();
        }
    }

    /**
     * Passes the given event to the queue, to be constructed in place.
     *
     * Errors if a memory allocation fails while pushing the event into the
     * queue.
     */
    template<typename... Args>
    void emplace(Args&&... args)
    {
        // Push the event into the queue.
        if (!(queue.emplace(std::forward<Args>(args)...))) {
            std::cout << "Memory allocation failed while pushing an event."
                      << std::endl;
            std::abort();
        }
    }

    /** A reference to our parent dispatcher. */
    EventDispatcher& dispatcher;

    /** The event queue. Holds events that have been pushed into it by the
        dispatcher. */
    moodycamel::BlockingReaderWriterQueue<T> queue;
};

//--------------------------------------------------------------------------
// EventSorter
//--------------------------------------------------------------------------
/**
 * Base class
 */
class EventSorterBase
{
public:
    /**
     * Indicates the validity of a given message's tick in relation to the
     * currentTick.
     */
    enum class ValidityResult
    {
        NotSet,
        /** The given message's tick was less than the MessageSorter's
           currentTick. */
        TooLow,
        /** The message's tick is valid. */
        Valid,
        /** The given message's tick was beyond the end of the buffer. */
        TooHigh
    };

    /**
     * The ValidityResult and associated diff from a push() operation.
     */
    struct PushResult {
        ValidityResult result{ValidityResult::NotSet};
        int64_t diff{0};
    };
};

/**
 * Sorts messages into an appropriate queue based on the tick number they're
 * associated with.
 *
 * Thread-safe, the intended usage is for an asynch receiver thread to act as
 * the producer, and for the main game loop to periodically consume the messages
 * for its current tick.
 *
 * To consume: Call startReceive, process all messages from the queue, then call
 * endReceive.
 * Producer note: Push will block until the consumer lock is released.
 */
template<typename T>
class EventSorter : public EventSorterBase
{
public:
    /**
     * The max valid positive difference between an incoming tick number and
     * our currentTick that we'll accept.
     * E.g. if this == 10, the valid range is [currentTick, currentTick + 10).
     *
     * Effectively, how far into the future we'll buffer messages for.
     */
    static constexpr std::size_t BUFFER_SIZE = 10;

    /** The range of difference (inclusive) between a received message's tickNum
       and our currentTick that we'll push a message for.
       Outside of the bounds, we'll drop the message. */
    static constexpr int MESSAGE_DROP_BOUND_LOWER = 0;
    static constexpr int MESSAGE_DROP_BOUND_UPPER = BUFFER_SIZE - 1;

    EventSorter(EventDispatcher& inDispatcher, uint32_t currentTick = 0)
    : dispatcher{inDispatcher}
    , currentTick(currentTick)
    , head(0)
    , isReceiving(false)
    {
        dispatcher.subscribe(this);
    }

    ~EventSorter()
    {
        // Note: This acquires a write lock, don't destruct this queue unless
        //       you don't mind potentially waiting.
        dispatcher.unsubscribe<T>(this);
    }

    /**
     * Starts a receive operation.
     *
     * NOTE: If tickNum is valid, locks the MessageSorter until endReceive is
     *       called.
     *
     * Returns a pointer to the queue holding messages for the given tick
     * number. If tickNum < currentTick or tickNum > (currentTick +
     * VALID_DIFFERENCE), it is considered not valid and an error occurs.
     *
     * @return If tickNum is valid (not too new or old), returns a reference to
     *         a queue. Else, errors.
     */
    std::queue<T>& startReceive(uint32_t tickNum)
    {
        if (isReceiving) {
            std::cout << "Tried to startReceive twice in a row. You probably "
                      << "forgot to call endReceive.";
            std::abort();
        }

        // Acquire the mutex.
        mutex.lock();

        // Check if the tick is valid.
        if (isTickValid(tickNum) != ValidityResult::Valid) {
            std::cout << "Tried to start receive for an invalid tick number." << std::endl;
            std::abort();
        }

        // Flag that we've started the receive operation.
        isReceiving = true;

        return queueBuffer[tickNum % BUFFER_SIZE];
    }

    /**
     * Ends an ongoing receive operation.
     *
     * NOTE: This function releases the lock set by startReceive.
     *
     * Increments currentTick and head, invalidating the old currentTick and
     * making messages at currentTick + BUFFER_SIZE - 1 valid to be pushed.
     *
     * @post The index previously pointed to by head is now head - 1,
     *       effectively making it the new end of the buffer.
     */
    void endReceive()
    {
        if (!isReceiving) {
            std::cout << "Tried to endReceive() while not receiving." << std::endl;
        }

        // Advance the state.
        head++;
        currentTick++;

        // Flag that we're ending the receive operation.
        isReceiving = false;

        // Release the mutex.
        mutex.unlock();
    }

    /**
     * Helper for checking if a tick number is within the bounds.
     *
     * Mostly used internally and for unit testing.
     */
    ValidityResult isTickValid(uint32_t tickNum)
    {
        // Check if tickNum is within our lower and upper bounds.
        uint32_t upperBound = (currentTick + BUFFER_SIZE - 1);
        if (tickNum < currentTick) {
            return ValidityResult::TooLow;
        }
        else if (tickNum > upperBound) {
            return ValidityResult::TooHigh;
        }
        else {
            return ValidityResult::Valid;
        }
    }

    /**
     * Returns the MessageSorter's internal currentTick.
     *
     * NOTE: Should not be used to fetch the current tick, get a ref to the
     *       Game's currentTick instead. This is just for unit testing.
     */
    uint32_t getCurrentTick() { return currentTick; }

private:
    /** Only give EventDispatcher access to push(), so users don't get confused
        about how to use the interface. */
    friend class EventDispatcher;

    /**
     * If tickNum is valid, buffers the message.
     *
     * Note: Blocks on mutex if there's a receive ongoing.
     *
     * @return True if tickNum was valid and the message was pushed, else false.
     */
    PushResult push(const T& message, uint32_t tickNum)
    {
        /** Try to push the message. */
        // Acquire the mutex.
        mutex.lock();

        // Check validity of the message's tick.
        ValidityResult validity = isTickValid(tickNum);

        // If tickNum is valid, push the message.
        if (validity == ValidityResult::Valid) {
            queueBuffer[tickNum % BUFFER_SIZE].push(message);
        }

        // Calc the tick diff.
        int64_t diff
            = static_cast<int64_t>(tickNum) - static_cast<int64_t>(currentTick);

        // Release the mutex.
        mutex.unlock();

        return {validity, diff};
    }

    /** A reference to our parent dispatcher. */
    EventDispatcher& dispatcher;

    /**
     * Holds the queues used for sorting and storing messages.
     *
     * Holds messages at an index equal to their tick number - currentTick
     * (e.g. if currentTick is 42, queueBuffer[0] holds messages for tick
     * number 42, queueBuffer[1] holds tick 43, etc.)
     */
    std::array<std::queue<T>, BUFFER_SIZE> queueBuffer;

    /**
     * The current tick that we've advanced to.
     */
    uint32_t currentTick;

    /**
     * The index at which we're holding the current tick.
     */
    std::size_t head;

    /**
     * Used to prevent queueBuffer and head updates while a push or receive is
     * happening.
     */
    std::mutex mutex;

    /**
     * Tracks whether a receive operation has been started or not.
     * Note: Not thread safe. Only call from the same thread as startReceive()
     *       and endReceive().
     */
    bool isReceiving;

    //----------------------------------------------------------------------------
    // Convenience Functions
    //----------------------------------------------------------------------------
    /**
     * Returns the index, incremented by amount. Accounts for wrap-around.
     */
    std::size_t increment(const std::size_t index,
                          const std::size_t amount) const
    {
        return (index + amount) % BUFFER_SIZE;
    }
};

} // End namespace AM
