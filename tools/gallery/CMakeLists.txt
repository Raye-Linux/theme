# raye-gallery: the visual test board of the theme (TH-16). A developer
# tool: it is not part of any package. tools/shoot.sh builds it when it
# is missing and captures every page under Xvfb.
#
#   cmake -S tools/gallery -B ~/.cache/raye-gallery-build -G Ninja
#   cmake --build ~/.cache/raye-gallery-build
#   ctest --test-dir ~/.cache/raye-gallery-build --output-on-failure
#
# Build it outside the repo (as above): the repo ignores no build folder.

cmake_minimum_required(VERSION 3.22)

project(raye-gallery LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Qml Quick QuickWidgets QuickControls2)
find_package(KF6Config REQUIRED)
find_package(KF6ColorScheme REQUIRED)

include(CTest)

add_library(raye-gallery-core STATIC
    src/gallery.cpp
    src/options.cpp
    src/page.cpp
    src/qmlpage.cpp
    src/referencepages.cpp
    src/scheme.cpp
    src/widgetpage.cpp
    src/widgetpages.cpp
    src/widgetpages2.cpp
    src/window.cpp
)
target_include_directories(raye-gallery-core PUBLIC src)
# where the repo's colors/ and qml/ are found when the program is built elsewhere
target_compile_definitions(raye-gallery-core PRIVATE RAYE_GALLERY_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(raye-gallery-core PRIVATE -Wall -Wextra)
target_link_libraries(raye-gallery-core PUBLIC
    Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::QuickWidgets Qt6::QuickControls2
    KF6::ConfigCore KF6::ColorScheme
)

set(gallery_qml
    qml/PageBase.qml
    qml/SampleRow.qml
    qml/QButtons.qml
    qml/QText.qml
    qml/QChoices.qml
    qml/QRanges.qml
    qml/QLists.qml
    qml/QMenus.qml
    qml/QDialogs.qml
    qml/QRaye.qml
)
qt_add_resources(raye-gallery-core gallery_qml PREFIX /gallery FILES ${gallery_qml})

add_executable(raye-gallery src/main.cpp)
target_compile_options(raye-gallery PRIVATE -Wall -Wextra)
target_link_libraries(raye-gallery PRIVATE raye-gallery-core)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    # the theme repo, for its colours/ and qml/
    get_filename_component(RAYE_THEME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    foreach(t options scheme pages)
        add_executable(tst_${t} tests/tst_${t}.cpp)
        target_compile_definitions(tst_${t} PRIVATE RAYE_THEME_DIR="${RAYE_THEME_DIR}")
        target_link_libraries(tst_${t} PRIVATE raye-gallery-core Qt6::Test)
        add_test(NAME ${t} COMMAND tst_${t})
        set_tests_properties(${t} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    endforeach()
    add_test(NAME shoot COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shoot.sh
        ${CMAKE_CURRENT_SOURCE_DIR}/../shoot.sh $<TARGET_FILE:raye-gallery>)
    set_tests_properties(shoot PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
endif()
