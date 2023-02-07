cmake_minimum_required(VERSION 3.16)

message(STATUS "Configuring Unit Tests")

# Configure Catch2.
if (NOT TARGET Catch2::Catch2)
    message(STATUS "QueuedEvents: Downloading dependency if not present: Catch2")

    SET(CATCH_BUILD_TESTING OFF CACHE BOOL "Build SelfTest project")
    SET(CATCH_INSTALL_DOCS OFF CACHE BOOL "Install documentation alongside library")
    include(FetchContent)
    FetchContent_Declare(Catch2Download
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.3.1.tar.gz
        URL_HASH MD5=5cdc99f93e0b709936eb5af973df2a5c
    )
    FetchContent_MakeAvailable(Catch2Download)
endif()

# Add the test executable target.
add_executable(QueuedEventsTests
    Private/TestMain.cpp
    Private/TestQueuedEvents.cpp
)

# Include our headers.
target_include_directories(QueuedEventsTests
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Public
)

# Link our dependencies.
target_link_libraries(QueuedEventsTests
    PRIVATE
        QueuedEvents
        Catch2::Catch2
)

# Compile with C++20
target_compile_features(QueuedEventsTests PRIVATE cxx_std_20)
set_target_properties(QueuedEventsTests PROPERTIES CXX_EXTENSIONS OFF)

# Enable compile warnings.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(QueuedEventsTests PUBLIC -Wall -Wextra)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(QueuedEventsTests PUBLIC /W3 /permissive-)
endif()
