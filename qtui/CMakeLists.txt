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
    include/qtui/tmessagebox.h
    include/qtui/tcheckbox.h
    include/qtui/tradiobutton.h
    include/qtui/tpushbutton.h
    include/qtui/tlabel.h
    include/qtui/tlineedit.h
    include/qtui/tcombobox.h
    include/qtui/tprogressbar.h
    include/qtui/tslider.h
    include/qtui/tgroupbox.h
    include/qtui/tdialog.h
    src/widget.cpp
    src/tmessagebox.cpp
    src/application.cpp
    src/tcheckbox.cpp
    src/tradiobutton.cpp
    src/tpushbutton.cpp
    src/tlabel.cpp
    src/tlineedit.cpp
    src/tcombobox.cpp
    src/tprogressbar.cpp
    src/tslider.cpp
    src/tgroupbox.cpp
    src/tdialog.cpp
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
        ncursesw
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
