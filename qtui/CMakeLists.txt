cmake_minimum_required(VERSION 3.21)
project(qtui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Curses REQUIRED)

add_library(qtui SHARED
    include/qtui/widget.h
    include/qtui/application.h
    include/qtui/messagebox.h
    include/qtui/checkbox.h
    include/qtui/radiobutton.h
    include/qtui/pushbutton.h
    include/qtui/label.h
    include/qtui/lineedit.h
    include/qtui/combobox.h
    include/qtui/progressbar.h
    include/qtui/groupbox.h
    include/qtui/dialog.h
    src/widget.cpp
    src/messagebox.cpp
    src/application.cpp
    src/checkbox.cpp
    src/radiobutton.cpp
    src/pushbutton.cpp
    src/label.cpp
    src/lineedit.cpp
    src/combobox.cpp
    src/progressbar.cpp
    src/groupbox.cpp
    src/dialog.cpp
)

target_include_directories(qtui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CURSES_INCLUDE_DIR}
)

target_link_libraries(qtui
    PUBLIC
        Qt6::Core
    PRIVATE
        ${CURSES_LIBRARIES}
)

target_compile_options(qtui PRIVATE
    -Wall -Wextra -Wpedantic
)

add_subdirectory(tests)

install(TARGETS qtui
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/qtui
    DESTINATION include
)
