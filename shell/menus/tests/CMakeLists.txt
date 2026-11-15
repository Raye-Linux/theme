# menucategories: builds a private menu cache from the fixtures with the
# Raye menu and checks where every app lands. The same tool lists the
# installed apps on a running system (list-installed.sh).

add_executable(raye-menu-categories menucategories.cpp)
target_link_libraries(raye-menu-categories Qt6::Core KF6::Service)

find_package(Python3 COMPONENTS Interpreter)

add_test(NAME menus-categories
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/run-menu-test.sh
        $<TARGET_FILE:raye-menu-categories> ${CMAKE_CURRENT_SOURCE_DIR}/..)

if(Python3_FOUND)
    add_test(NAME menus-generated
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../gen-menu.py --check)
endif()
