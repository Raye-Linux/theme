# raye-polkit-agent: the Raye admin prompt.
#
#   src/       the agent (polkit listener on libpolkit-agent-1, PAM conversation,
#              app lookup)
#   qml/       the prompt, built into the binary
#   tests/     app lookup and request logic (QtTest), the prompt (QML test)
#
# The prompt draws with the sign-in module org.raye.signin of raye-theme;
# the tests take it from ../../signin.

cmake_minimum_required(VERSION 3.22)
project(raye-polkit-agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core DBus Gui Qml Quick)
find_package(PkgConfig REQUIRED)
pkg_check_modules(POLKIT_AGENT REQUIRED IMPORTED_TARGET polkit-agent-1)
find_package(LayerShellQt REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

include(CTest)

add_library(raye-polkit-core STATIC src/appinfo.cpp src/request.cpp)
target_include_directories(raye-polkit-core PUBLIC src)
target_link_libraries(raye-polkit-core PUBLIC Qt6::Core)

# the windows and the QML prompt, shared by the agent and the preview
qt_add_library(raye-polkit-ui STATIC src/prompt.cpp src/iconprovider.h)
qt_add_resources(raye-polkit-ui prompt
    PREFIX /qt/qml/org/raye/polkitagent
    BASE qml
    FILES qml/Prompt.qml qml/PromptButton.qml
)
target_link_libraries(raye-polkit-ui PUBLIC
    raye-polkit-core
    Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick
    LayerShellQt::Interface
)

add_executable(raye-polkit-agent src/main.cpp src/listener.cpp)
target_link_libraries(raye-polkit-agent PRIVATE raye-polkit-ui PkgConfig::POLKIT_AGENT)
# glib's headers use "signals" as a name
target_compile_definitions(raye-polkit-agent PRIVATE QT_NO_KEYWORDS QT_NO_CAST_FROM_ASCII)

install(TARGETS raye-polkit-agent DESTINATION libexec)
install(FILES raye-polkit-agent.service DESTINATION lib/systemd/user)
install(FILES plasma-polkit-agent.conf DESTINATION lib/systemd/user/plasma-polkit-agent.service.d
        RENAME raye-polkit-agent.conf)
install(FILES plasma-workspace.conf DESTINATION lib/systemd/user/plasma-workspace.target.d
        RENAME raye-polkit-agent.conf)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)

    foreach(t appinfo request)
        add_executable(tst_${t} tests/tst_${t}.cpp)
        target_link_libraries(tst_${t} PRIVATE raye-polkit-core Qt6::Test)
        add_test(NAME ${t} COMMAND tst_${t})
    endforeach()

    # the real windows with a pretend PAM, to look at (not a test)
    add_executable(preview tests/preview.cpp)
    target_link_libraries(preview PRIVATE raye-polkit-ui)

    # the prompt, with a stand-in for the agent's objects
    # HINTS, not PATHS. CMake searches HINTS BEFORE the system PATH and
    # PATHS AFTER it -- and what sits on Ubuntu's PATH is
    # /usr/bin/qmltestrunner, a symlink to qtchooser that defaults to Qt 5:
    #   qmltestrunner: could not exec '/usr/lib/qt5/bin/qmltestrunner'
    # Measured 2026-11-06 in the resolute chroot. With PATHS this test was
    # one Qt 5 tool away from running the wrapper instead of the runner, on
    # the very platform Raye is built on. qml/CMakeLists.txt already had it
    # right; this file was the odd one out.
    # REQUIRED, and `if(QMLTESTRUNNER)` is gone with it. Without REQUIRED
    # a missing runner left QMLTESTRUNNER as QMLTESTRUNNER-NOTFOUND, the
    # `if` was false, and `prompt` was simply NEVER REGISTERED -- while
    # appinfo and request still were. Measured 2026-11-06 on a two-test
    # tree: the configure exits 0, and `ctest` with
    # CTEST_NO_TESTS_ACTION=error prints "100% tests passed, 0 tests failed
    # out of 1" and exits 0. The variable cannot see this: one is not zero,
    # exactly as two is not zero in settings' glob. With REQUIRED the same
    # absence exits 1 at configure time, measured on the same tree.
    # qml/CMakeLists.txt already had REQUIRED; this file was the odd one out.
    find_program(QMLTESTRUNNER qmltestrunner
        HINTS /usr/lib/qt6/bin /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/qt6/bin
        REQUIRED)
    add_test(NAME prompt
        COMMAND ${QMLTESTRUNNER} -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_prompt.qml
                -import ${CMAKE_CURRENT_SOURCE_DIR}/../../signin
                -import ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(prompt PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
