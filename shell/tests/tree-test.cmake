# Builds a copy of shell/CMakeLists.txt with made-up parts and checks what
# it installs: applet packages without their tests, compiled applets and
# parts built from their own CMakeLists.txt, grouped parts, translations,
# and nothing for folders that have not landed yet. Then the same file
# with no part at all must build too.
#
#   cmake -DSHELL_SOURCE=<shell> -DFIXTURES=<fixtures> -DWORK=<dir> -DGENERATOR=Ninja -P tree-test.cmake

foreach(var SHELL_SOURCE FIXTURES WORK GENERATOR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

file(REMOVE_RECURSE ${WORK})

function(run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE out)
    if(code)
        message(FATAL_ERROR "failed (${code}): ${ARGN}\n${out}")
    endif()
endfunction()

# Configure, build and install a tree into <work>/root.
function(build_tree name)
    set(src ${WORK}/${name}/shell)
    set(bin ${WORK}/${name}/build)
    run(${CMAKE_COMMAND} -S ${src} -B ${bin} -G ${GENERATOR}
        -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF)
    run(${CMAKE_COMMAND} --build ${bin})
    run(${CMAKE_COMMAND} -E env DESTDIR=${WORK}/${name}/root ${CMAKE_COMMAND} --install ${bin})
endfunction()

function(expect name path)
    if(NOT EXISTS ${WORK}/${name}/root/${path})
        file(GLOB_RECURSE got RELATIVE ${WORK}/${name}/root ${WORK}/${name}/root/*)
        message(FATAL_ERROR "${name}: ${path} was not installed. Installed:\n${got}")
    endif()
endfunction()

function(expect_not name path)
    if(EXISTS ${WORK}/${name}/root/${path})
        message(FATAL_ERROR "${name}: ${path} must not be installed")
    endif()
endfunction()

function(marker dir dest)
    file(WRITE ${dir}/marker "${dest}\n")
    file(WRITE ${dir}/CMakeLists.txt "install(FILES marker DESTINATION share/raye-tree-test RENAME ${dest})\n")
endfunction()

# --- a tree with one of each kind of part ---------------------------------
set(S ${WORK}/full/shell)
file(MAKE_DIRECTORY ${S})
file(COPY ${SHELL_SOURCE}/CMakeLists.txt DESTINATION ${S})
file(WRITE ${S}/README.md "not a part\n")

# an applet made of QML, with things next to it that never ship
set(A ${S}/plasmoids/org.raye.fixture.good)
file(COPY ${FIXTURES}/plasma/plasmoids/org.raye.fixture.good/ DESTINATION ${A})
file(WRITE ${A}/README.md "notes\n")
file(WRITE ${A}/Messages.sh "#!/bin/sh\n")
file(WRITE ${A}/tests/CMakeLists.txt "message(FATAL_ERROR \"applet tests must not build without BUILD_TESTING\")\n")
file(WRITE ${A}/contents/ui/main.qmlc "cache")

# an applet that has not landed yet: a folder with no metadata.json
file(MAKE_DIRECTORY ${S}/plasmoids/org.raye.unfinished/contents/ui)
file(WRITE ${S}/plasmoids/org.raye.unfinished/contents/ui/main.qml "import QtQuick\nItem {}\n")

# a compiled applet builds and installs itself
file(MAKE_DIRECTORY ${S}/plasmoids/org.raye.compiled)
file(WRITE ${S}/plasmoids/org.raye.compiled/metadata.json "{}\n")
marker(${S}/plasmoids/org.raye.compiled compiled)

# a part at the top, a part inside a group folder, and empty folders
file(MAKE_DIRECTORY ${S}/direct ${S}/kwin/org.raye.part ${S}/kwin/org.raye.notyet ${S}/empty)
marker(${S}/direct direct)
marker(${S}/kwin/org.raye.part grouped)

file(COPY ${FIXTURES}/po DESTINATION ${S})

build_tree(full)
set(P usr/share/plasma/plasmoids)
expect(full ${P}/org.raye.fixture.good/metadata.json)
expect(full ${P}/org.raye.fixture.good/contents/ui/main.qml)
expect(full ${P}/org.raye.fixture.good/contents/config/main.xml)
expect_not(full ${P}/org.raye.fixture.good/README.md)
expect_not(full ${P}/org.raye.fixture.good/Messages.sh)
expect_not(full ${P}/org.raye.fixture.good/tests)
expect_not(full ${P}/org.raye.fixture.good/contents/ui/main.qmlc)
expect_not(full ${P}/org.raye.unfinished)
expect_not(full ${P}/org.raye.compiled)
expect(full usr/share/raye-tree-test/compiled)
expect(full usr/share/raye-tree-test/direct)
expect(full usr/share/raye-tree-test/grouped)
expect(full usr/share/locale/fr/LC_MESSAGES/raye_shell_fixture.mo)

file(GLOB_RECURSE installed RELATIVE ${WORK}/full/root ${WORK}/full/root/*)
list(LENGTH installed count)
# metadata.json, main.qml, main.xml, three markers, one .mo
if(NOT count EQUAL 7)
    message(FATAL_ERROR "full: expected 7 installed files, got ${count}:\n${installed}")
endif()

# --- the tree as it is before any part lands ------------------------------
file(MAKE_DIRECTORY ${WORK}/empty/shell/plasmoids)
file(COPY ${SHELL_SOURCE}/CMakeLists.txt DESTINATION ${WORK}/empty/shell)
build_tree(empty)
file(GLOB_RECURSE installed ${WORK}/empty/root/*)
if(installed)
    message(FATAL_ERROR "empty: installed ${installed}")
endif()

file(REMOVE_RECURSE ${WORK})
message(STATUS "tree: ok")
