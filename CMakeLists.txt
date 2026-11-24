cmake_minimum_required(VERSION 3.22)
project(RobotPathPlanning VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RPP_BUILD_GUI "Build the SFML 3.x desktop GUI application" ON)
option(RPP_BUILD_TESTS "Build the core unit & integration test suite" ON)
option(RPP_STATIC_SFML "Link SFML statically into the executable (recommended for single-EXE Windows portability)" OFF)

# Core robotics simulation library (pure C++17, zero GUI dependencies)
add_library(rpp_core STATIC
    src/Node.cpp
    src/StaticObstacle.cpp
    src/DynamicObstacle.cpp
    src/Grid.cpp
    src/AStar.cpp
    src/Dijkstra.cpp
    src/Robot.cpp
    src/EventLogger.cpp
    src/Statistics.cpp
    src/ExplanationEngine.cpp
    src/ScenarioManager.cpp
    src/Simulation.cpp
)

target_include_directories(rpp_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(rpp_core PRIVATE /W4)
else()
    target_compile_options(rpp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(RPP_BUILD_GUI)
    if(RPP_STATIC_SFML)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
    else()
        set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries (DLLs)" FORCE)
    endif()

    include(FetchContent)
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_DOC OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        3.0.0
        GIT_SHALLOW    ON
    )
    FetchContent_MakeAvailable(SFML)

    add_executable(RobotPathPlanning
        src/main.cpp
        src/Application.cpp
        src/UIManager.cpp
    )

    if(WIN32)
        # Hide extra console window in Release builds on Windows
        set_target_properties(RobotPathPlanning PROPERTIES
            WIN32_EXECUTABLE $<IF:$<CONFIG:Release>,TRUE,FALSE>
        )
    endif()

    target_link_libraries(RobotPathPlanning PRIVATE
        rpp_core
        SFML::Graphics
        SFML::Window
        SFML::System
    )

    # Copy assets (if present) and scenarios next to RobotPathPlanning.exe after build
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
        add_custom_command(TARGET RobotPathPlanning POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/assets
                $<TARGET_FILE_DIR:RobotPathPlanning>/assets
        )
    endif()
    add_custom_command(TARGET RobotPathPlanning POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/scenarios
            $<TARGET_FILE_DIR:RobotPathPlanning>/scenarios
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/README.txt
            $<TARGET_FILE_DIR:RobotPathPlanning>/README.txt
    )

    # On Windows with shared SFML, copy runtime DLLs automatically into target output dir
    if(WIN32 AND NOT RPP_STATIC_SFML)
        add_custom_command(TARGET RobotPathPlanning POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:RobotPathPlanning>
                $<TARGET_FILE_DIR:RobotPathPlanning>
            COMMAND_EXPAND_LISTS
        )
    endif()
endif()

if(RPP_BUILD_TESTS)
    enable_testing()
    add_executable(rpp_tests tests/test_core.cpp)
    target_link_libraries(rpp_tests PRIVATE rpp_core)
    add_test(NAME CoreRoboticsSuite COMMAND rpp_tests)
endif()
