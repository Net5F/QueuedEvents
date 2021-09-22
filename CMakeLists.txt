cmake_minimum_required(VERSION 3.16)

project(QueuedEvents)

###############################################################################
# Options
###############################################################################
option(QUEUEDEVENTS_BUILD_TESTS "Build tests." ON)

###############################################################################
# Dependencies 
###############################################################################
# If not already provided, configure header-only readerwriterqueue.
if (NOT TARGET readerwriterqueue)
    add_subdirectory("${PROJECT_SOURCE_DIR}/Libraries/readerwriterqueue/"
                     "${PROJECT_BINARY_DIR}/Libraries/readerwriterqueue/")
endif()

###############################################################################
# Modules
###############################################################################
# Configure the library target.
add_subdirectory(Source)

# Configure the tests target.
if (QUEUEDEVENTS_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
