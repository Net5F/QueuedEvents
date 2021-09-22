cmake_minimum_required(VERSION 3.16)

message(STATUS "Configuring Unit Tests")

# Configure Catch2.
SET(CATCH_BUILD_TESTING OFF CACHE BOOL "Build SelfTest project")
SET(CATCH_INSTALL_DOCS OFF CACHE BOOL "Install documentation alongside library")
add_subdirectory("${PROJECT_SOURCE_DIR}/Libraries/Catch2/"
                 "${PROJECT_BINARY_DIR}/Libraries/Catch2/")

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
target_compile_options(QueuedEventsTests PUBLIC -Wall -Wextra)
