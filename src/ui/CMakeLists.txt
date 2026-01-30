cmake_minimum_required(VERSION 3.21)
project(ui-wrapper-test VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# NOTE: qtui library is added by parent CMakeLists.txt
# Do NOT add it again here to avoid duplicate target errors

# UI Wrapper library
add_library(ui-wrappers STATIC
    context.h
    context.cpp
    qcheckbox.h
    qcheckbox.cpp
    qradiobutton.h
    qradiobutton.cpp
    qpushbutton.h
    qpushbutton.cpp
    qlineedit.h
    qlineedit.cpp
    qcombobox.h
    qcombobox.cpp
    qlabel.h
    qlabel.cpp
    qprogressbar.h
    qprogressbar.cpp
    qmessagebox.h
    qmessagebox.cpp
)

target_include_directories(ui-wrappers
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../qtui/include
)

target_link_libraries(ui-wrappers
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        qtui
)

# Test executable
add_executable(test-wrapper test-wrapper.cpp)

target_link_libraries(test-wrapper
    PRIVATE
        ui-wrappers
        Qt6::Core
        Qt6::Widgets
        qtui
        ncurses
)
