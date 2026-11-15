foreach(_test runparser runhistory)
    add_executable(tst_${_test} tst_${_test}.cpp)
    target_link_libraries(tst_${_test} raye-run-core Qt6::Test)
    add_test(NAME run-${_test} COMMAND tst_${_test})
    set_tests_properties(run-${_test} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

# the once-per-user shortcut repair (skipped where the KDE config tools
# are not installed)
add_test(NAME run-shortcut-update COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tst_shortcut_update.sh
         ${CMAKE_CURRENT_SOURCE_DIR}/../data/raye-run-shortcut.sh)
set_tests_properties(run-shortcut-update PROPERTIES SKIP_RETURN_CODE 77)

# Win+R is asked for by the Run box's .desktop
add_test(NAME run-desktop-shortcut COMMAND grep -qx "X-KDE-Shortcuts=Meta+R"
         ${CMAKE_CURRENT_BINARY_DIR}/../org.raye.run.desktop)

# Browse... must not take the box down (Qt's quit lock)
add_executable(tst_runapp tst_runapp.cpp ../src/runapp.cpp)
target_include_directories(tst_runapp PRIVATE ../src)
target_link_libraries(tst_runapp Qt6::Widgets)
add_test(NAME run-runapp COMMAND tst_runapp)
set_tests_properties(run-runapp PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
