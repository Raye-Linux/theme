add_library(rayecommon${QT_MAJOR_VERSION} OBJECT rayeboxshadowrenderer.cpp)

set_target_properties(rayecommon${QT_MAJOR_VERSION} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(rayecommon${QT_MAJOR_VERSION}
    PUBLIC
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Gui)
