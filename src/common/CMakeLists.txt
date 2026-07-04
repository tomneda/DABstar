set(commonLibName ${objectName}_common)

search_for_library(LIBSNDFILE sndfile)

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
        ${LIBSNDFILE_LIBRARIES}
)

if (extraLibDirs)
  target_link_directories(${commonLibName} PUBLIC ${extraLibDirs})
endif ()
