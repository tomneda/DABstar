cmake_minimum_required(VERSION 3.16)
project(dumpViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt Designer's promoted PlotWidget lives in the forms directory
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../../forms)

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)

add_executable(dumpViewer
    main.cpp
    dump_viewer.h
    dump_viewer.cpp
    ../../scopes/plot_widget.h
    ../../scopes/plot_widget.cpp
    ../../../forms/dumpwidget.ui
)

target_include_directories(dumpViewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}           # dump_viewer.h, ui_dumpwidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scopes  # plot_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../main    # glob_data_types.h, glob_defs.h (make_vla)
)

target_link_libraries(dumpViewer PRIVATE
    Qt6::Widgets
    Qt6::Charts
)

set_target_properties(dumpViewer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/linux-bin
)
