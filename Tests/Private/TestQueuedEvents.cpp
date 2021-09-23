#include "catch2/catch_all.hpp"
#include "QueuedEvents.h"
#include <thread>

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
    
    SECTION("Complex notify/receive across threads")
    {
        // TODO: Write these concurrency tests, and write a readme with
        //       an example.
    }
}
