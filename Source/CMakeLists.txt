cmake_minimum_required(VERSION 3.16)

message(STATUS "Configuring QueuedEvents")

# Add the library target.
add_library(QueuedEvents
    PUBLIC
        Public/QueuedEvents.h
)

# Include our headers.
target_include_directories(QueuedEvents
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Public
)

# Link our dependencies.
target_link_libraries(QueuedEvents
    PUBLIC
        readerwriterqueue
)

# Tell CMake we're using C++ (it has trouble detecting it).
set_target_properties(QueuedEvents PROPERTIES LINKER_LANGUAGE CXX)

# Compile with C++20.
target_compile_features(QueuedEvents PRIVATE cxx_std_20)
set_target_properties(QueuedEvents PROPERTIES CXX_EXTENSIONS OFF)

# Enable compile warnings.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(QueuedEvents PUBLIC -Wall -Wextra)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(QueuedEvents PUBLIC /W4 /permissive-)
endif()
