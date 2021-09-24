#include "catch2/catch_all.hpp"
#include "QueuedEvents.h"
#include <thread>
#include <chrono>

using namespace AM;

struct TestStruct1
{
    unsigned int temp1{};
};

struct TestStruct2
{
    float temp2{};
};

TEST_CASE("TestQueuedEvents")
{
    EventDispatcher dispatcher;

    // Note: This test is to see if it errors or anything. It may not fail 
    //       cleanly.
    SECTION("Construct/destruct queue")
    {
        {
            EventQueue<TestStruct1> queue(dispatcher);
            REQUIRE(dispatcher.getNumQueuesForType<TestStruct1>() == 1);
        }
        REQUIRE(dispatcher.getNumQueuesForType<TestStruct1>() == 0);
    }

    SECTION("Notify single type once")
    {
        // Construct the queue.
        EventQueue<TestStruct1> queue(dispatcher);
        REQUIRE(queue.size() == 0);

        // Notify once.
        dispatcher.notify<TestStruct1>(std::make_shared<const TestStruct1>(10));
        REQUIRE(queue.size() == 1);

        // Check if we got the event.
        std::shared_ptr<const TestStruct1> testEvent = queue.pop();
        REQUIRE(testEvent != nullptr);
        REQUIRE(testEvent->temp1 == 10);
        REQUIRE(queue.size() == 0);
    }

    SECTION("Notify single type multiple times")
    {
        // Construct the queue.
        EventQueue<TestStruct1> queue(dispatcher);

        // Notify multiple times.
        for (unsigned int i = 0; i < 5; ++i) {
            dispatcher.notify<TestStruct1>(std::make_shared<const TestStruct1>(i));
        }
        REQUIRE(queue.size() == 5);

        // Check if we got the events.
        for (unsigned int i = 0; i < 5; ++i) {
            std::shared_ptr<const TestStruct1> testEvent = queue.pop();
            REQUIRE(testEvent != nullptr);
            REQUIRE(testEvent->temp1 == i);
        }
        REQUIRE(queue.size() == 0);
    }

    SECTION("Notify multiple types once")
    {
        // Construct the TestStruct1 queue.
        EventQueue<TestStruct1> queue(dispatcher);

        // Notify TestStruct1 once.
        dispatcher.notify<TestStruct1>(std::make_shared<const TestStruct1>(10));
        REQUIRE(queue.size() == 1);

        // Check if we got the event.
        std::shared_ptr<const TestStruct1> testEvent = queue.pop();
        REQUIRE(testEvent != nullptr);
        REQUIRE(testEvent->temp1 == 10);
        REQUIRE(queue.size() == 0);

        // Construct the TestStruct2 queue.
        EventQueue<TestStruct2> queue2(dispatcher);

        // Notify TestStruct2 once.
        dispatcher.notify<TestStruct2>(std::make_shared<const TestStruct2>(20.0f));
        REQUIRE(queue2.size() == 1);

        // Check if we got the event.
        std::shared_ptr<const TestStruct2> testEvent2 = queue2.pop();
        REQUIRE(testEvent2 != nullptr);
        REQUIRE(testEvent2->temp2 == 20.0f);
        REQUIRE(queue2.size() == 0);
    }

    SECTION("Notify multiple types multiple times")
    {
        // Construct the TestStruct1 queue.
        EventQueue<TestStruct1> queue(dispatcher);

        // Notify TestStruct1 multiple times.
        for (unsigned int i = 0; i < 5; ++i) {
            dispatcher.notify<TestStruct1>(std::make_shared<const TestStruct1>(i * 10));
        }
        REQUIRE(queue.size() == 5);

        // Construct the TestStruct2 queue.
        EventQueue<TestStruct2> queue2(dispatcher);

        // Notify TestStruct2 multiple times.
        for (unsigned int i = 0; i < 5; ++i) {
            dispatcher.notify<TestStruct2>(std::make_shared<const TestStruct2>(i * 20));
        }
        REQUIRE(queue2.size() == 5);

        // Check if we got the events.
        for (unsigned int i = 0; i < 5; ++i) {
            std::shared_ptr<const TestStruct1> testEvent = queue.pop();
            REQUIRE(testEvent != nullptr);
            REQUIRE(testEvent->temp1 == (i * 10));

            std::shared_ptr<const TestStruct2> testEvent2 = queue2.pop();
            REQUIRE(testEvent2 != nullptr);
            REQUIRE(testEvent2->temp2 == static_cast<float>((i * 20)));
        }
        REQUIRE(queue.size() == 0);
        REQUIRE(queue2.size() == 0);
    }

    SECTION("Notify multiple queues.")
    {
        // Construct the first queue.
        EventQueue<TestStruct1> queue1(dispatcher);

        // Construct the second queue.
        EventQueue<TestStruct1> queue2(dispatcher);
        REQUIRE(dispatcher.getNumQueuesForType<TestStruct1>() == 2);

        // Notify multiple times.
        for (unsigned int i = 0; i < 5; ++i) {
            dispatcher.notify<TestStruct1>(std::make_shared<const TestStruct1>(i));
        }
        REQUIRE(queue1.size() == 5);
        REQUIRE(queue2.size() == 5);

        // Check if we got the events.
        for (unsigned int i = 0; i < 5; ++i) {
            std::shared_ptr<const TestStruct1> testEvent1 = queue1.pop();
            REQUIRE(testEvent1 != nullptr);
            REQUIRE(testEvent1->temp1 == i);

            std::shared_ptr<const TestStruct1> testEvent2 = queue2.pop();
            REQUIRE(testEvent2 != nullptr);
            REQUIRE(testEvent2->temp1 == i);
        }
        REQUIRE(queue1.size() == 0);
        REQUIRE(queue2.size() == 0);
    }
    
    SECTION("Notify/receive across threads")
    {
        // Construct our queues.
        EventQueue<TestStruct1> queue1(dispatcher);
        EventQueue<TestStruct2> queue2(dispatcher);
        
        // Create our notification thread.
        unsigned int eventsToSend{100};
        std::thread notifyThread([&dispatcher, eventsToSend]() {
            std::shared_ptr<TestStruct1> testStruct1(std::make_shared<TestStruct1>(10));
            std::shared_ptr<TestStruct2> testStruct2(std::make_shared<TestStruct2>(20));
            
            // Notify a bunch of times.
            for (unsigned int i = 0; i < eventsToSend; ++i) {
                dispatcher.notify<TestStruct1>(testStruct1);

                // We sleep for some small amount of time in a lazy attempt 
                // to shake out any timing issues.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));

                dispatcher.notify<TestStruct2>(testStruct2);

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

        // Receive the events.
        unsigned int struct1Received{0};
        unsigned int struct2Received{0};
        while ((struct1Received < eventsToSend)
               && (struct2Received < eventsToSend)) {
            if ((struct1Received != eventsToSend) 
               && (queue1.pop() != nullptr)) {
                struct1Received++;
            }

            if ((struct2Received != eventsToSend) 
               && (queue2.pop() != nullptr)) {
                struct2Received++;
            }
        }
        
        notifyThread.join();
        REQUIRE(true);
    }

    SECTION("Notify and construct at the same time")
    {
        // Create our notification thread.
        unsigned int eventsToSend{10'000};
        std::atomic<bool> start{false};
        std::thread notifyThread([&dispatcher, &start, eventsToSend]() {
            std::shared_ptr<TestStruct1> testStruct1(std::make_shared<TestStruct1>(10));
            
            // Busy wait for the main thread to be ready.
            while (!start) {
            }
            
            // Notify a bunch of times.
            for (unsigned int i = 0; i < eventsToSend; ++i) {
                dispatcher.notify<TestStruct1>(testStruct1);
            }
        });
        
        // Kick off the notify thread and start constructing queues.
        start = true;
        for (unsigned int i = 0; i < eventsToSend; ++i) {
            EventQueue<TestStruct1> queue(dispatcher);
        }

        notifyThread.join();
        REQUIRE(true);
    }
}

// This test is hidden. Run it with "./QueuedEventsTests.exe TestPerformance"
TEST_CASE("TestPerformance", "[.]")
{
    /**
     * In this test, we run our event system against a raw moodycamel reader 
     * writer queue.
     *
     * Test steps:
     *     (Pre-test) Prepare a struct.
     *     1. Push the struct into the queue.
     *     2. Pop the struct.
     *
     * You can control the number of times the above steps are performed 
     * through the iterationCount variable.
     */
    SECTION("Compare performance with raw queue.")
    {
        // The number of times to run data through each queue.
        unsigned int iterationCount{1'000'000};
        std::cout << "Running " << iterationCount << " structs through the queue." << std::endl;
        
        // The test struct to use.
        std::shared_ptr<TestStruct1> testStruct(std::make_shared<TestStruct1>(1));

        /** Raw queue. */
        // Construct a queue.
        moodycamel::ReaderWriterQueue<std::shared_ptr<const TestStruct1>> rwQueue;
        
        // Run the test.
        unsigned int count{0};
        auto startTime{std::chrono::high_resolution_clock::now()};
        for (unsigned int i = 0; i < iterationCount; ++i) {
            // Push
            rwQueue.enqueue(testStruct);
            
            // Increment
            count += testStruct->temp1;
            
            // Pop
            std::shared_ptr<const TestStruct1> receiveStruct{nullptr};
            rwQueue.try_dequeue(receiveStruct);
            REQUIRE(receiveStruct != nullptr);
        }
        auto stopTime{std::chrono::high_resolution_clock::now()};
        auto queueTestTime{duration_cast<std::chrono::milliseconds>(stopTime - startTime)};
        
        // Check that the iterations occurred correctly.
        REQUIRE(count == iterationCount);

        /** EventQueue/EventDispatcher. */
        // Construct our dispatcher and queue.
        EventDispatcher dispatcher;
        EventQueue<TestStruct1> eventQueue(dispatcher);

        // Run the test.
        count = 0;
        startTime = std::chrono::high_resolution_clock::now();
        for (unsigned int i = 0; i < iterationCount; ++i) {
            // Push
            dispatcher.notify<TestStruct1>(testStruct);
            
            // Increment
            count += testStruct->temp1;
            
            // Pop
            std::shared_ptr<const TestStruct1> receiveStruct{eventQueue.pop()};
            REQUIRE(receiveStruct != nullptr);
        }
        stopTime = std::chrono::high_resolution_clock::now();
        auto dispatcherTestTime{duration_cast<std::chrono::milliseconds>(stopTime - startTime)};
        
        // Check that the iterations occurred correctly.
        REQUIRE(count == iterationCount);
        
        // Print the results.
        std::cout << "Raw queue test time: " << queueTestTime.count() << std::endl;
        std::cout << "EventQueue/EventDispatcher test time: " 
                  << dispatcherTestTime.count() << std::endl;
    }
}
