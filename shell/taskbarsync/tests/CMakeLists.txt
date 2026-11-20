add_executable(tst_taskbarsync tst_taskbarsync.cpp)
target_link_libraries(tst_taskbarsync PRIVATE Qt6::Test Qt6::Qml)
target_compile_definitions(tst_taskbarsync PRIVATE RAYE_TASKBARSYNC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/..")
add_test(NAME taskbarsync COMMAND tst_taskbarsync)
set_tests_properties(taskbarsync PROPERTIES TIMEOUT 60)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME taskbarsync-pins COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/test_pins.py)
add_test(NAME taskbarsync-unit COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/test_unit.py)
