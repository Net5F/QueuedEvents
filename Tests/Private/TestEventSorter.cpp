#include "catch2/catch_all.hpp"
#include "QueuedEvents.h"
#include <thread>
#include <chrono>

using namespace AM;

struct TestStruct1 {
    unsigned int temp1{};
};

struct TestStruct2 {
    float temp2{};
};

TEST_CASE("TestEventSorter")
{
    EventDispatcher dispatcher;

    // Note: This test is to see if it errors or anything. It may not fail
    //       cleanly.
    SECTION("Construct/destruct sorter")
    {
        {
            EventSorter<TestStruct1> sorter(dispatcher);
            REQUIRE(dispatcher.getNumSortersForType<TestStruct1>() == 1);
            REQUIRE(sorter.getCurrentTick() == 0);
        }
        REQUIRE(dispatcher.getNumSortersForType<TestStruct1>() == 0);
    }

    SECTION("Push single type once")
    {
        // Construct the sorter.
        EventSorter<TestStruct1> sorter(dispatcher, 42);

        // Push once.
        TestStruct1 testStruct{10};
        uint32_t tickNum{42};
        dispatcher.push<TestStruct1>(testStruct, tickNum);

        // Check if we got the event.
        moodycamel::ReaderWriterQueue<TestStruct1>& queue = sorter.startReceive(42);
        REQUIRE(queue.size_approx() == 1);

        TestStruct1 testEvent{};
        REQUIRE(queue.try_dequeue(testEvent));
        REQUIRE(testEvent.temp1 == 10);
        
        // Advance the tick.
        sorter.endReceive();
        REQUIRE(sorter.isTickValid(42) == EventSorterBase::ValidityResult::TooLow);
    }
}
