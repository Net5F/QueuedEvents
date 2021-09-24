cmake_minimum_required(VERSION 3.16)

message(STATUS "Configuring QueuedEvents")

# Add the library target.
add_library(QueuedEvents
    INTERFACE
        Public/QueuedEvents.h
)

# Include our headers.
target_include_directories(QueuedEvents
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/Public
)

# Link our dependencies.
target_link_libraries(QueuedEvents
    INTERFACE
        readerwriterqueue
)

# Tell CMake we're using C++ (it has trouble detecting it).
set_target_properties(QueuedEvents PROPERTIES LINKER_LANGUAGE CXX)

# Compile with C++20.
target_compile_features(QueuedEvents INTERFACE cxx_std_20)
set_target_properties(QueuedEvents PROPERTIES CXX_EXTENSIONS OFF)

# Enable compile warnings.
target_compile_options(QueuedEvents INTERFACE -Wall -Wextra)
