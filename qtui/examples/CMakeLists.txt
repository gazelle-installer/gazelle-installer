cmake_minimum_required(VERSION 3.21)
project(qtui-examples VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Curses REQUIRED)

# Add parent directory to include path (for development)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../build)

# Login form example
add_executable(login-form login-form.cpp)
target_link_libraries(login-form
    qtui
    Qt6::Core
    ${CURSES_LIBRARIES}
)

# Installer settings example
add_executable(installer-settings installer-settings.cpp)
target_link_libraries(installer-settings
    qtui
    Qt6::Core
    ${CURSES_LIBRARIES}
)

# Progress demo example
add_executable(progress-demo progress-demo.cpp)
target_link_libraries(progress-demo
    qtui
    Qt6::Core
    ${CURSES_LIBRARIES}
)
