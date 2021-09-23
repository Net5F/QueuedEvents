#pragma once

#include "readerwriterqueue.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
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
 * Be aware:
 *   EventQueue uses a single producer/single consumer queue, so it isn't
 *   safe to notify the same type across multiple threads. Notifying different
 *   types should be fine.
 *
 *   Dispatcher's subscribe() and unsubscribe() acquire a write (unique) lock
 *   over the internal data structure. While they're running, notify() will
 *   block.
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
     * Pushes the given event to all queues subscribed to receive events of
     * type T.
     */
    template <typename T>
    void notify(const std::shared_ptr<const T>& event)
    {
        // Acquire a read lock before accessing a queue.
        std::shared_lock readLock(queueVectorMapMutex);

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
     * Returns a new event queue, subscribed to receive notifications for
     * events of type T.
     */
    template <typename T>
    void subscribe(EventQueue<T>* queuePtr)
    {
        // Get the vector of queues for type T.
        QueueVector& queueVector{getQueueVectorForType<T>()};

        // Acquire a write lock before modifying the vector.
        std::unique_lock lock(queueVectorMapMutex);

        // Add the given queue to the vector.
        // Allocate the new queue and add it to the vector.
        queueVector.push_back(static_cast<void*>(queuePtr));
    }

    /**
     * Unsubscribes the given event queue. It will no longer receive event
     * notifications.
     */
    template<typename T>
    void unsubscribe(const EventQueue<T>* unsubQueuePtr)
    {
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

        // Acquire a write lock before modifying the vector.
        std::unique_lock lock(queueVectorMapMutex);

        // Remove the queue at the given index.
        // Note: This may do some extra work to fill the gap, but there's
        //       probably only 2-3 elements and shared_ptrs are small.
        queueVector.erase(queueIt);
    }

    /** A map of integer keys -> vectors of event queues.
        We store each EventQueue<T> as a void pointer so that they can share
        a vector, then cast them to the appropriate type in our templated
        functions. */
    std::unordered_map<int, QueueVector> queueVectorMap;

    /** Used to lock access to the queueVectorMap. */
    std::shared_mutex queueVectorMapMutex;
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
     * Attempts to pop the top event from the queue.
     *
     * @return An event if the queue was non-empty, else nullptr;
     */
    std::shared_ptr<const T> pop()
    {
        // Try to pop an event.
        std::shared_ptr<const T> event;
        if (queue.try_dequeue(event)) {
            // We had an event to pop, return it.
            return event;
        }
        else {
            // The queue was empty.
            return nullptr;
        }
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
    void push(const std::shared_ptr<const T>& event)
    {
        // Push the event into the queue.
        if (!(queue.enqueue(event))) {
            std::cout << "Memory allocation failed while pushing an event." << std::endl;
            std::abort();
        }
    }

    /** A reference to our parent dispatcher. */
    EventDispatcher& dispatcher;

    /** The event queue. Holds events that have been pushed into it by the
        dispatcher. */
    moodycamel::ReaderWriterQueue<std::shared_ptr<const T>> queue;
};

} // End namespace AM
