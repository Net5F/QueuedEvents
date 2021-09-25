#pragma once

#include "readerwriterqueue.h"
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdlib>
#include <utility>
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
 *   notify(), subscribe(), and unsubscribe() will block if either of the
 *   other two are running. This means they're safe to call across threads,
 *   but may not be perfectly performant.
 *
 * Note: It would be ideal to have the two classes in separate headers, but
 *       we would run into circular include issues.
 */

// Forward declaration
template <typename T>
class EventQueue;

//--------------------------------------------------------------------------
// EventDispatcher
//--------------------------------------------------------------------------
/**
 * A simple event dispatcher.
 *
 * Through notify<T>(), dispatches events to any subscribed EventQueues of a
 * matching type T.
 */
class EventDispatcher
{
public:
    // See queueVectorMap comment for details.
    using QueueVector = std::vector<void*>;

    /**
     * Pushes the given event to all queues of type T.
     */
    template <typename T>
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
     * Constructs the given event in place in all queues of type T.
     */
    template <typename T, typename... Args>
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
     * Returns the number of queues that are currently constructed for type T.
     *
     * Not thread safe. Really only useful for testing.
     */
    template <typename T>
    std::size_t getNumQueuesForType()
    {
        // Get the vector of queues for type T.
        QueueVector& queueVector{getQueueVectorForType<T>()};

        // Return the number of queues.
        return queueVector.size();
    }

private:
    /** Only give EventQueue access to subscribe() and unsubscribe(), so users
        don't get confused about how to use the interface. */
    template<typename> friend class EventQueue;

    /**
     * Returns the next unique integer key value. Used by getKeyForType().
     */
    static int getNextKey()
    {
        static std::atomic<int> nextKey{0};
        return nextKey++;
    }

    /**
     * Returns the vector of queues that hold type T.
     */
    template <typename T>
    QueueVector& getQueueVectorForType()
    {
        // Get the key associated with type T.
        // Note: Static var means that getNextKey() will only be called once
        //       per type T, and that this key will be consistent across all
        //       Observer instances.
        static int key{getNextKey()};

        // Return the vector at the given key.
        // (Constructs one if there is no existing vector.)
        return queueVectorMap[key];
    }

    /**
     * Subscribes the given queue to receive event notifications.
     *
     * Only used by EventQueue.
     */
    template <typename T>
    void subscribe(EventQueue<T>* queuePtr)
    {
        // Acquire a lock since we're going to be modifying data structures.
        std::scoped_lock lock(queueVectorMapMutex);

        // Get the vector of queues for type T.
        QueueVector& queueVector{getQueueVectorForType<T>()};

        // Add the given queue to the vector.
        // Allocate the new queue and add it to the vector.
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
            std::cout << "Failed to find queue while unsubscribing." << std::endl;
            std::abort();
        }

        // Remove the queue at the given index.
        // Note: This may do some extra work to fill the gap, but there's
        //       probably only 2-3 elements and pointers are small.
        queueVector.erase(queueIt);
    }

    /** A map of integer keys -> vectors of event queues.
        We store each EventQueue<T> as a void pointer so that they can share
        a vector, then cast them to the appropriate type in our templated
        functions. */
    std::unordered_map<int, QueueVector> queueVectorMap;

    /** Used to lock access to the queueVectorMap. */
    std::mutex queueVectorMapMutex;
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
template <typename T>
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
    bool pop()
    {
        return queue.pop();
    }

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
    T* peek() const
    {
        return queue.peek();
    }

    /**
     * Returns the number of elements in the queue.
     */
    std::size_t size()
    {
        return queue.size_approx();
    }

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
            std::cout << "Memory allocation failed while pushing an event." << std::endl;
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
            std::cout << "Memory allocation failed while pushing an event." << std::endl;
            std::abort();
        }
    }

    /** A reference to our parent dispatcher. */
    EventDispatcher& dispatcher;

    /** The event queue. Holds events that have been pushed into it by the
        dispatcher. */
    moodycamel::BlockingReaderWriterQueue<T> queue;
};

} // End namespace AM
