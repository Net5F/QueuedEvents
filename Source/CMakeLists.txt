cmake_minimum_required(VERSION 3.16)

message(STATUS "Configuring QueuedEvents")

# Add the library target.
add_library(QueuedEvents INTERFACE)

# Include our headers.
target_include_directories(QueuedEvents
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/Public
)

# Include our sources.
target_sources(QueuedEvents INTERFACE
    Public/QueuedEvents.h
)

# Link our dependencies.
target_link_libraries(QueuedEvents
    INTERFACE
        readerwriterqueue
)

# Compile with C++20.
target_compile_features(QueuedEvents INTERFACE cxx_std_20)

# Enable compile warnings.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(QueuedEvents INTERFACE -Wall -Wextra)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(QueuedEvents INTERFACE /W3 /permissive-)
endif()
