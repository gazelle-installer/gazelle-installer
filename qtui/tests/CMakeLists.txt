add_executable(test-tui
    test-tui.cpp
)

target_link_libraries(test-tui
    PRIVATE
        qtui
        Qt6::Core
        ${CURSES_LIBRARIES}
)
