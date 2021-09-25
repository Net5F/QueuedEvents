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

    SECTION("Push single type once")
    {
        // Construct the queue.
        EventQueue<TestStruct1> queue(dispatcher);
        REQUIRE(queue.size() == 0);

        // Push once.
        TestStruct1 testStruct{10};
        dispatcher.push<TestStruct1>(testStruct);
        REQUIRE(queue.size() == 1);

        // Check if we got the event.
        TestStruct1 testEvent;
        REQUIRE(queue.pop(testEvent));
        REQUIRE(testEvent.temp1 == 10);
        REQUIRE(queue.size() == 0);
    }

    SECTION("Move single type once")
    {
        // Construct the queue.
        EventQueue<TestStruct1> queue(dispatcher);
        REQUIRE(queue.size() == 0);

        // Push once.
        TestStruct1 testStruct{10};
        dispatcher.push<TestStruct1>(std::move(testStruct));
        REQUIRE(queue.size() == 1);

        // Check if we got the event.
        TestStruct1 testEvent;
        REQUIRE(queue.pop(testEvent));
        REQUIRE(testEvent.temp1 == 10);
        REQUIRE(queue.size() == 0);
    }

    SECTION("Emplace single type once")
    {
        // Construct the queue.
        EventQueue<TestStruct1> queue(dispatcher);
        REQUIRE(queue.size() == 0);

        // Push once.
        dispatcher.emplace<TestStruct1>(10);
        REQUIRE(queue.size() == 1);

        // Check if we got the event.
        TestStruct1 testEvent;
        REQUIRE(queue.pop(testEvent));
        REQUIRE(testEvent.temp1 == 10);
        REQUIRE(queue.size() == 0);
    }

    SECTION("Push single type multiple times")
    {
        // Construct the queue.
        EventQueue<TestStruct1> queue(dispatcher);

        // Push multiple times.
        for (unsigned int i = 0; i < 5; ++i) {
            dispatcher.emplace<TestStruct1>(i);
        }
        REQUIRE(queue.size() == 5);

        // Check if we got the events.
        for (unsigned int i = 0; i < 5; ++i) {
            TestStruct1 testEvent;
            REQUIRE(queue.pop(testEvent));
            REQUIRE(testEvent.temp1 == i);
        }
        REQUIRE(queue.size() == 0);
    }

    SECTION("Push multiple types once")
    {
        // Construct the TestStruct1 queue.
        EventQueue<TestStruct1> queue(dispatcher);

        // Push TestStruct1 once.
        dispatcher.emplace<TestStruct1>(10);
        REQUIRE(queue.size() == 1);

        // Check if we got the event.
        TestStruct1 testEvent;
        REQUIRE(queue.pop(testEvent));
        REQUIRE(testEvent.temp1 == 10);
        REQUIRE(queue.size() == 0);

        // Construct the TestStruct2 queue.
        EventQueue<TestStruct2> queue2(dispatcher);

        // Push TestStruct2 once.
        dispatcher.emplace<TestStruct2>(20.0f);
        REQUIRE(queue2.size() == 1);

        // Check if we got the event.
        TestStruct2 testEvent2;
        REQUIRE(queue2.pop(testEvent2));
        REQUIRE(testEvent2.temp2 == 20.0f);
        REQUIRE(queue2.size() == 0);
    }

    SECTION("Push multiple types multiple times")
    {
        // Construct the TestStruct1 queue.
        EventQueue<TestStruct1> queue(dispatcher);

        // Push TestStruct1 multiple times.
        for (unsigned int i = 0; i < 5; ++i) {
            dispatcher.emplace<TestStruct1>(i * 10);
        }
        REQUIRE(queue.size() == 5);

        // Construct the TestStruct2 queue.
        EventQueue<TestStruct2> queue2(dispatcher);

        // Push TestStruct2 multiple times.
        for (unsigned int i = 0; i < 5; ++i) {
            dispatcher.emplace<TestStruct2>(i * 20);
        }
        REQUIRE(queue2.size() == 5);

        // Check if we got the events.
        for (unsigned int i = 0; i < 5; ++i) {
            TestStruct1 testEvent;
            REQUIRE(queue.pop(testEvent));
            REQUIRE(testEvent.temp1 == (i * 10));

            TestStruct2 testEvent2;
            REQUIRE(queue2.pop(testEvent2));
            REQUIRE(testEvent2.temp2 == static_cast<float>((i * 20)));
        }
        REQUIRE(queue.size() == 0);
        REQUIRE(queue2.size() == 0);
    }

    SECTION("Push multiple queues.")
    {
        // Construct the first queue.
        EventQueue<TestStruct1> queue1(dispatcher);

        // Construct the second queue.
        EventQueue<TestStruct1> queue2(dispatcher);
        REQUIRE(dispatcher.getNumQueuesForType<TestStruct1>() == 2);

        // Push multiple times.
        for (unsigned int i = 0; i < 5; ++i) {
            dispatcher.emplace<TestStruct1>(i);
        }
        REQUIRE(queue1.size() == 5);
        REQUIRE(queue2.size() == 5);

        // Check if we got the events.
        for (unsigned int i = 0; i < 5; ++i) {
            TestStruct1 testEvent1;
            REQUIRE(queue1.pop(testEvent1));
            REQUIRE(testEvent1.temp1 == i);

            TestStruct1 testEvent2;
            REQUIRE(queue2.pop(testEvent2));
            REQUIRE(testEvent2.temp1 == i);
        }
        REQUIRE(queue1.size() == 0);
        REQUIRE(queue2.size() == 0);
    }
    
    SECTION("Push/receive across threads")
    {
        // Construct our queues.
        EventQueue<TestStruct1> queue1(dispatcher);
        EventQueue<TestStruct2> queue2(dispatcher);
        
        // Create our notification thread.
        unsigned int eventsToSend{100};
        std::thread pushThread([&dispatcher, eventsToSend]() {
            TestStruct1 testStruct1{10};
            TestStruct2 testStruct2{20};
            
            // Push a bunch of times.
            for (unsigned int i = 0; i < eventsToSend; ++i) {
                dispatcher.push<TestStruct1>(testStruct1);

                // We sleep for some small amount of time in a lazy attempt 
                // to shake out any timing issues.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));

                dispatcher.push<TestStruct2>(testStruct2);

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

        // Receive the events.
        unsigned int struct1Received{0};
        unsigned int struct2Received{0};
        TestStruct1 testStruct1;
        TestStruct2 testStruct2;
        while ((struct1Received < eventsToSend)
               && (struct2Received < eventsToSend)) {
            if ((struct1Received != eventsToSend) 
               && queue1.pop(testStruct1)) {
                struct1Received++;
            }

            if ((struct2Received != eventsToSend) 
               && queue2.pop(testStruct2)) {
                struct2Received++;
            }
        }
        
        pushThread.join();
        REQUIRE(true);
    }

    // This test will crash if it fails.
    SECTION("Push and construct at the same time")
    {
        // Create our notification thread.
        unsigned int eventsToSend{10'000};
        std::atomic<bool> start{false};
        std::thread pushThread([&dispatcher, &start, eventsToSend]() {
            TestStruct1 testStruct1{10};
            
            // Busy wait for the main thread to be ready.
            while (!start) {
            }
            
            // Push a bunch of times.
            for (unsigned int i = 0; i < eventsToSend; ++i) {
                dispatcher.push<TestStruct1>(testStruct1);
            }
        });
        
        // Kick off the push thread and start constructing queues.
        start = true;
        for (unsigned int i = 0; i < eventsToSend; ++i) {
            EventQueue<TestStruct1> queue(dispatcher);
        }

        pushThread.join();
        REQUIRE(true);
    }
    
    SECTION("Peek")
    {
        // Construct the queue.
        EventQueue<TestStruct1> queue(dispatcher);
        REQUIRE(queue.size() == 0);

        // Push events.
        for (unsigned int i = 1; i <= 3; ++i) {
            dispatcher.emplace<TestStruct1>(i);
            REQUIRE(queue.size() == i);
        }
        
        // Peek and pop the events.
        for (unsigned int i = 1; i <= 3; ++i) {
            TestStruct1* testStruct{queue.peek()};
            REQUIRE(testStruct != nullptr);
            REQUIRE(testStruct->temp1 == i);
            
            queue.pop();
            REQUIRE(queue.size() == (3 - i));
        }
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
        TestStruct1 testStruct{1};

        /** Raw queue. */
        // Construct a queue.
        moodycamel::ReaderWriterQueue<TestStruct1> rwQueue;
        
        // Run the test.
        unsigned int count{0};
        auto startTime{std::chrono::high_resolution_clock::now()};
        for (unsigned int i = 0; i < iterationCount; ++i) {
            // Push
            rwQueue.enqueue(testStruct);
            
            // Increment
            count += testStruct.temp1;
            
            // Pop
            TestStruct1 receiveStruct;
            REQUIRE(rwQueue.try_dequeue(receiveStruct));
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
            dispatcher.push<TestStruct1>(testStruct);
            
            // Increment
            count += testStruct.temp1;
            
            // Pop
            TestStruct1 receiveStruct;
            REQUIRE(eventQueue.pop(receiveStruct));
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
