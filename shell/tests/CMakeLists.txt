# Tests of the shell tree. Run by ctest, and so by the package build.
#
#   plasmoidcheck   the checks themselves, on the broken applets in fixtures/
#   plasmoidload    every applet of shell/plasmoids: metadata, French
#                   strings, QML, then a real load in plasmawindowed
#   tree            a copy of the top CMakeLists.txt with made-up parts:
#                   what it builds and installs, and that an empty tree builds

add_library(raye_shell_check STATIC plasmoidcheck.cpp plasmoidrun.cpp plasmoidcheck.h)
target_link_libraries(raye_shell_check PUBLIC Qt6::Core Qt6::Qml)
target_include_directories(raye_shell_check PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# The runs need no display, but they need these.
find_program(RAYE_DBUS_RUN_SESSION dbus-run-session REQUIRED)
find_program(RAYE_PLASMAWINDOWED plasmawindowed REQUIRED)

set(_raye_test_env
    "QT_QPA_PLATFORM=offscreen"
    "QT_QUICK_BACKEND=software"
    # the tests write nothing into the real home
    "HOME=${CMAKE_CURRENT_BINARY_DIR}/home"
    "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config"
    "XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")

add_executable(tst_plasmoidcheck tst_plasmoidcheck.cpp)
target_link_libraries(tst_plasmoidcheck PRIVATE raye_shell_check Qt6::Test Qt6::Gui)
target_compile_definitions(tst_plasmoidcheck PRIVATE
    RAYE_SHELL_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME plasmoidcheck COMMAND tst_plasmoidcheck)
set_tests_properties(plasmoidcheck PROPERTIES ENVIRONMENT "${_raye_test_env}" TIMEOUT 300)

# The id list is the one the top CMakeLists.txt installs.
# joined with commas: a ";" would split the definition in two
list(JOIN RAYE_SHELL_PLASMOID_IDS "," _raye_ids)
add_executable(tst_plasmoidload tst_plasmoidload.cpp)
target_link_libraries(tst_plasmoidload PRIVATE raye_shell_check Qt6::Test Qt6::Gui)
target_compile_definitions(tst_plasmoidload PRIVATE
    "RAYE_SHELL_PLASMOID_IDS=\"${_raye_ids}\""
    RAYE_SHELL_PLASMOIDS_DIR="${RAYE_SHELL_PLASMOIDS_DIR}"
    RAYE_SHELL_PO_DIR="${RAYE_SHELL_PO_DIR}"
    RAYE_SHELL_BUILD_DIR="${CMAKE_BINARY_DIR}"
    RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
add_test(NAME plasmoidload COMMAND tst_plasmoidload)
# an applet takes about 10 s (two runs); a stuck one must not hold the build
set_tests_properties(plasmoidload PROPERTIES ENVIRONMENT "${_raye_test_env}" TIMEOUT 1500)

add_test(NAME tree
    COMMAND ${CMAKE_COMMAND}
        -DSHELL_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/..
        -DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/tree
        -DGENERATOR=${CMAKE_GENERATOR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tree-test.cmake)
set_tests_properties(tree PROPERTIES ENVIRONMENT "${_raye_test_env}" TIMEOUT 600)
