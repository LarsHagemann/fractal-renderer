cmake_minimum_required(VERSION 3.21)
project(fractal-renderer)

find_package(OpenGL REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PARALLEL_COUNT 16)
message(STATUS "Parallel count: ${PARALLEL_COUNT}")

add_subdirectory(SFML)

add_executable(
    ${PROJECT_NAME}

    main.cpp

    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui.cpp
    imgui-sfml/imgui-SFML.cpp
)
target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    sfml-graphics
    sfml-audio
    sfml-system
    sfml-window

    ${OPENGL_LIBRARIES}
)
target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
    SFML/include/
    imgui/
    imgui-sfml/
)
target_compile_definitions(
    ${PROJECT_NAME}
    PUBLIC
    PARALLEL_COUNT=${PARALLEL_COUNT}
)

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR})
install(
    TARGETS ${PROJECT_NAME}
    DESTINATION bin
    RUNTIME
)
