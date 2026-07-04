set(commonLibName ${objectName}_common)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)

find_package(Qt6Core REQUIRED)
find_package(Qt6Widgets REQUIRED)
find_package(Qt6Xml REQUIRED)

add_library(${commonLibName} STATIC
        fir_filters.cpp
        openfiledialog.cpp
        xml_filewriter.cpp
        setting_helper.cpp
)

target_include_directories(${commonLibName}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${commonLibName}
        PUBLIC
        Qt6::Widgets
        Qt6::Xml
)
