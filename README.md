# A simple C++ event system that pushes events into thread-safe queues.
This event system satisfies a simple use case--sending events from a producer thread into consumer queues that can later be processed.

For me, this means my network thread can push messages (and other network events) down to my simulation's systems, and those systems can process those events during the next tick.

## Features
* Uses moodycamel::ReaderWriterQueue under the hood. Very fast, well tested, and well maintained.
* Pretty fast itself, only about twice the overhead of pushing/popping from the raw queue. 
  * Down to 1.4x if I can figure out how to get rid of the locks.
* Very, very simple interface. One line each to construct a dispatcher, construct and subscribe a queue, notify, and receive.
* Handles any arbitrary type with no type registration or overhead.
  * This is a downside if you like to have a list of available types, but you can always do an IDE lookup on EventDispatcher::notify().
* Well-commented, understandable, maintainable code.
  * It's somewhat complex due to the nature of template metaprogramming, but I prioritize keeping things well documented.

## Example
```
#include "QueuedEvents.h"

struct TestStruct
{
  int count{0};
};

// Construct the dispatcher (normally done in a producer context and made 
// available to the consumer context through a getter).
EventDispatcher dispatcher;

// Construct the queue (subscribes itself using the dispatcher reference).
EventQueue<TestStruct> testStructQueue(dispatcher);

// Push an event to all subscribed queues (must be wrapped in a shared_ptr).
std::shared_ptr<TestStruct> testStruct = std::make_shared<TestStruct>(10);
dispatcher.notify<TestStruct>(testStruct);

// Receive the event (becomes const so that multiple receivers can't change
// eachother's received data).
std::shared_ptr<const TestStruct> receivedStruct = testStructQueue.pop();
```

## Build
I use git submodules and CMake, but everything is in a small single header, so you can just copy QueuedEvents.h directly to your project if you prefer.
