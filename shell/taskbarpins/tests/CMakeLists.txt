add_executable(tst_pinrules tst_pinrules.cpp ../src/pinrules.cpp)
target_include_directories(tst_pinrules PRIVATE ../src)
target_link_libraries(tst_pinrules PRIVATE Qt6::Test)
add_test(NAME taskbarpins COMMAND tst_pinrules)
