set(commonLibName ${objectName}_common)

search_for_library(LIBSNDFILE sndfile)

set(${commonLibName}_SRCS
        fir_filters.cpp
        openfiledialog.cpp
        xml_filewriter.cpp
        setting_helper.cpp
)

if (USE_LIQUID)
    list(APPEND ${commonLibName}_SRCS halfbandfilter.cpp)
endif ()

add_library(${commonLibName} STATIC ${${commonLibName}_SRCS})

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

# Propagate the liquid-dsp library PUBLIC so every consumer of common (base,
# devices and any future sub library) inherits it when USE_LIQUID is enabled.
if (USE_LIQUID)
    target_link_libraries(${commonLibName} PUBLIC liquid)
endif ()

if (extraLibDirs)
  target_link_directories(${commonLibName} PUBLIC ${extraLibDirs})
endif ()
