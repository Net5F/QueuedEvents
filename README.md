# A simple C++ event system that pushes events into thread-safe queues.
This event system satisfies a simple use case--sending events from a producer thread into consumer queues that can later be processed.

For me, this means my network thread can push messages (and other network events) down to my simulation's systems, and those systems can process those events during the next tick.

## Features
* Uses moodycamel::ReaderWriterQueue under the hood. Very fast, well tested, and well maintained.
* Pretty fast itself, only about 1.5x the overhead of pushing/popping from the raw queue. 
  * Performance has only lightly been tested, YMMV.
* Very, very simple interface. One line each to construct a dispatcher, construct and subscribe a queue, notify, and receive.
* Handles any arbitrary type with no type registration or overhead.
  * This is a downside if you like to have a list of available types, but you can always do an IDE lookup on EventDispatcher::push() or enqueue().
* Well-commented, understandable, maintainable code.
  * It's somewhat complex due to the nature of template metaprogramming, but I prioritize keeping things well documented.

## Example
```
#include "QueuedEvents.h"

struct Foo
{
  int count{0};
};

// Construct the dispatcher (normally done in a producer context and made 
// available to the consumer context through a getter).
AM::EventDispatcher dispatcher;

// Construct the queue (subscribes itself using the dispatcher reference).
AM::EventQueue<Foo> queue(dispatcher);

// Push an event to all subscribed queues.
Foo foo{10};
dispatcher.push<Foo>(foo);

// You can also move or emplace.
dispatcher.push<Foo>(std::move(foo));

dispatcher.emplace<Foo>(10);

// Receive the event.
Foo receivedFoo;
bool result = queue.pop(receivedFoo); // Will be false if queue is empty.

// You can also peek and empty pop.
Foo* fooPtr = queue.peek(); // Will be nullptr if queue is empty.

queue.pop(); // Do this after you're done using the pointer.
```

## Build
I use git submodules and CMake, but everything is in a small single header, so you can just copy QueuedEvents.h directly to your project if you prefer.
