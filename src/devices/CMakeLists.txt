
set(devicesLibName ${objectName}_devices)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6Core REQUIRED)
find_package(Qt6Widgets REQUIRED)
find_package(Qt6Xml REQUIRED)

if (SDRPLAY_V2)
    include_directories(
            sdrplay-handler-v2
    )

    set($(devicesLibName)_HDRS
            ${${devicesLibName}_HDRS}
            sdrplay-handler-v2/sdrplay-handler-v2.h
            sdrplay-handler-v2/sdrplayselect.h
            sdrplay-handler-v2/mirsdrapi-rsp.h
    )

    set(${devicesLibName}_SRCS
            ${${devicesLibName}_SRCS}
            sdrplay-handler-v2/sdrplay-handler-v2.cpp
            sdrplay-handler-v2/sdrplayselect.cpp
    )

    add_definitions(-DHAVE_SDRPLAY_V2)
endif (SDRPLAY_V2)

if (SDRPLAY_V3)
    include_directories(
            sdrplay-handler-v3
    )

    set($(devicesLibName)_HDRS
            ${${devicesLibName}_HDRS}
            sdrplay-handler-v3/sdrplay-handler-v3.h
            sdrplay-handler-v3/sdrplay-commands.h
            sdrplay-handler-v3/Rsp-device.h
            sdrplay-handler-v3/Rsp1-handler.h
            sdrplay-handler-v3/Rsp1A-handler.h
            sdrplay-handler-v3/RspII-handler.h
            sdrplay-handler-v3/RspDuo-handler.h
            sdrplay-handler-v3/RspDx-handler.h
    )

    set(${devicesLibName}_SRCS
            ${${devicesLibName}_SRCS}
            sdrplay-handler-v3/sdrplay-handler-v3.cpp
            sdrplay-handler-v3/Rsp-device.cpp
	    sdrplay-handler-v3/Rsp1-handler.cpp
            sdrplay-handler-v3/Rsp1A-handler.cpp
            sdrplay-handler-v3/Rsp2-handler.cpp
            sdrplay-handler-v3/RspDuo-handler.cpp
            sdrplay-handler-v3/RspDx-handler.cpp
    )

    add_definitions(-DHAVE_SDRPLAY_V3)
endif (SDRPLAY_V3)

if (AIRSPY)
    # find_package(LibAIRSPY)
    # if (NOT LIBAIRSPY_FOUND)
    #   message(FATAL_ERROR "please install airspy library")
    # endif ()
    # ## include_directories (${AIRSPYLIB_INCLUDE_DIR})

    include_directories(
            airspy-handler
    )

    set($(devicesLibName)_HDRS
            ${${devicesLibName}_HDRS}
            airspy-handler/airspy-handler.h
            airspy-handler/airspyselect.h
    )

    set(${devicesLibName}_SRCS
            ${${devicesLibName}_SRCS}
            airspy-handler/airspy-handler.cpp
            airspy-handler/airspyselect.cpp
    )

    add_definitions(-DHAVE_AIRSPY)
endif (AIRSPY)

if (SPYSERVER)
    # find_package(LibAIRSPY)
    # if (NOT LIBAIRSPY_FOUND)
    #   message(FATAL_ERROR "please install airspy library")
    # endif ()
    # ## include_directories (${AIRSPYLIB_INCLUDE_DIR})

    include_directories(
            spy-server
    )

    set($(devicesLibName)_HDRS
            ${${devicesLibName}_HDRS}
            spy-server/spyserver-handler.h
            spy-server/spyserver-client.h
            spy-server/spyserver-protocol.h
            spy-server/spyserver-tcp-client.h
    )

    set(${devicesLibName}_SRCS
            ${${devicesLibName}_SRCS}
            spy-server/spyserver-handler.cpp
            spy-server/spyserver-client.cpp
            spy-server/spyserver-tcp-client.cpp
    )

    add_definitions(-DHAVE_SPYSERVER)
endif (SPYSERVER)

if (HACKRF)
    include_directories(
            hackrf-handler
    )

    set($(devicesLibName)_HDRS
            ${${devicesLibName}_HDRS}
            hackrf-handler/hackrf-handler.h
    )

    set(${devicesLibName}_SRCS
            ${${devicesLibName}_SRCS}
            hackrf-handler/hackrf-handler.cpp
    )

    add_definitions(-DHAVE_HACKRF)
endif (HACKRF)

if (PLUTO)
    include_directories(
            pluto-handler-2
    )

    set($(devicesLibName)_HDRS
            ${${devicesLibName}_HDRS}
            pluto-handler-2/pluto-handler.h
    )

    set(${devicesLibName}_SRCS
            ${${devicesLibName}_SRCS}
            pluto-handler-2/pluto-handler.cpp
    )

    add_definitions(-DHAVE_PLUTO)
endif (PLUTO)

if (LIMESDR)
    include_directories(
            lime-handler
    )

    set($(devicesLibName)_HDRS
            ${${devicesLibName}_HDRS}
            lime-handler/lime-handler.h
            lime-handler/lime-widget.h
            lime-handler/LimeSuite.h
            lime-handler/LMS7002M_parameters.h
    )

    set(${devicesLibName}_SRCS
            ${${devicesLibName}_SRCS}
            lime-handler/lime-handler.cpp
    )

    add_definitions(-DHAVE_LIME)
endif (LIMESDR)

if (RTLSDR)
    include_directories(
            rtlsdr-handler
    )

    set(${devicesLibName}_HDRS
            ${${devicesLibName}_HDRS}
            rtlsdr-handler/rtlsdr-handler.h
            rtlsdr-handler/rtl-dongleselect.h
    )

    set(${devicesLibName}_SRCS
            ${${devicesLibName}_SRCS}
            rtlsdr-handler/rtlsdr-handler.cpp
            rtlsdr-handler/rtl-dongleselect.cpp
    )

    add_definitions(-DHAVE_RTLSDR)
endif ()

if (UHD)
    find_package(UHD REQUIRED)
    if (NOT UHD_FOUND)
        message(FATAL_ERROR "please install UHD library")
    endif ()
    list(APPEND extraLibs ${UHD_LIBRARIES})

    include_directories(
            uhd/
    )

    set(${devicesLibName}_HDRS
            ${${devicesLibName}_HDRS}
            uhd/uhd-handler.h
    )

    set(${devicesLibName}_SRCS
            ${${devicesLibName}_SRCS}
            uhd/uhd-handler.cpp
    )

    add_definitions(-DHAVE_UHD)
endif ()

if (RTL_TCP)
    find_package(Qt6Network REQUIRED)
    include_directories(
            rtl_tcp
            ${Qt6Network_INCLUDE_DIRS}
    )

    set($(devicesLibName)_HDRS
            ${${devicesLibName}_HDRS}
            rtl_tcp/rtl_tcp_client.h
    )

    set(${devicesLibName}_SRCS
            ${${devicesLibName}_SRCS}
            rtl_tcp/rtl_tcp_client.cpp
    )

    set(RTLTCP_lib Qt6::Network)
    add_definitions(-DHAVE_RTL_TCP)
endif (RTL_TCP)

#---------------------------------------------------

include_directories(
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${QT6Widgets_INCLUDES}
        ${QT_QTCORE_INCLUDE_DIR}
        ${QT_QTGUI_INCLUDE_DIR}
        ${QT_QTCORE_INCLUDE_DIR}
        ../main
        ../support
        dummy-handler
        filereaders/filereader
        filereaders/xml-filereader
        filereaders/raw-files
        filereaders/wav-files
)


set($(devicesLibName)_HDRS
        ${${devicesLibName}_HDRS}
        device-selector.h
        filereaders/xml-filereader/element-reader.h
        filereaders/xml-filereader/xml-filereader.h
        filereaders/xml-filereader/xml-reader.h
        filereaders/xml-filereader/xml-descriptor.h
        filereaders/raw-files/rawfiles.h
        filereaders/raw-files/raw-reader.h
        filereaders/wav-files/wavfiles.h
        filereaders/wav-files/wav-reader.h
)

set(${devicesLibName}_SRCS
        ${${devicesLibName}_SRCS}
        device-selector.cpp
        filereaders/xml-filereader/xml-filereader.cpp
        filereaders/xml-filereader/xml-reader.cpp
        filereaders/xml-filereader/xml-descriptor.cpp
        filereaders/raw-files/rawfiles.cpp
        filereaders/raw-files/raw-reader.cpp
        filereaders/wav-files/wavfiles.cpp
        filereaders/wav-files/wav-reader.cpp
)

add_library(${devicesLibName} STATIC ${${devicesLibName}_SRCS} ${${devicesLibName}_HDRS})

target_link_libraries(${devicesLibName}
        PRIVATE
        Qt6::Widgets
        Qt6::Xml
        Qt6::Network
        ${extraLibs}
)

target_include_directories(${devicesLibName} INTERFACE .)
