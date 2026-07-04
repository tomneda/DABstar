
set(devicesLibName ${objectName}_devices)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)

find_package(Qt6Core REQUIRED)
find_package(Qt6Widgets REQUIRED)
find_package(Qt6Xml REQUIRED)

# UI files with no filtered preselection
set(devices_ui
        forms/colibri_widget.ui
        forms/elad_files.ui
        forms/elad_widget.ui
        forms/extio_widget.ui
        forms/xmlfiles.ui
)

if (SDRPLAY)
  include_directories(
          sdrplay_handler
  )

  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          sdrplay_handler/sdrplay_handler.h
          sdrplay_handler/sdrplay_commands.h
          sdrplay_handler/Rsp_device.h
          sdrplay_handler/Rsp1_handler.h
          sdrplay_handler/Rsp1A_handler.h
          sdrplay_handler/Rsp2_handler.h
          sdrplay_handler/RspDuo_handler.h
          sdrplay_handler/RspDx_handler.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          sdrplay_handler/sdrplay_handler.cpp
          sdrplay_handler/Rsp_device.cpp
          sdrplay_handler/Rsp1_handler.cpp
          sdrplay_handler/Rsp1A_handler.cpp
          sdrplay_handler/Rsp2_handler.cpp
          sdrplay_handler/RspDuo_handler.cpp
          sdrplay_handler/RspDx_handler.cpp
  )

  list(APPEND devices_ui forms/sdrplay_widget.ui)
  add_definitions(-DHAVE_SDRPLAY)
endif (SDRPLAY)

if (AIRSPY)
  # find_package(LibAIRSPY)
  # if (NOT LIBAIRSPY_FOUND)
  #   message(FATAL_ERROR "please install airspy library")
  # endif ()
  # ## include_directories (${AIRSPYLIB_INCLUDE_DIR})

  include_directories(
          airspy_handler
  )

  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          airspy_handler/airspy_handler.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          airspy_handler/airspy_handler.cpp
  )

  list(APPEND devices_ui forms/airspy_widget.ui)
  add_definitions(-DHAVE_AIRSPY)
endif (AIRSPY)

if (SPYSERVER)
  # find_package(LibAIRSPY)
  # if (NOT LIBAIRSPY_FOUND)
  #   message(FATAL_ERROR "please install airspy library")
  # endif ()
  # ## include_directories (${AIRSPYLIB_INCLUDE_DIR})

  include_directories(
          spy_server
  )

  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          spy_server/spyserver_handler.h
          spy_server/spyserver_client.h
          spy_server/spyserver_protocol.h
          spy_server/spyserver_tcp_client.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          spy_server/spyserver_handler.cpp
          spy_server/spyserver_client.cpp
          spy_server/spyserver_tcp_client.cpp
  )

  list(APPEND devices_ui forms/spyserver_client.ui)
  add_definitions(-DHAVE_SPYSERVER)
endif (SPYSERVER)

if (HACKRF)
  include_directories(
          hackrf_handler
  )

  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          hackrf_handler/hackrf_handler.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          hackrf_handler/hackrf_handler.cpp
  )

  list(APPEND devices_ui forms/hackrf_widget.ui)
  add_definitions(-DHAVE_HACKRF)
endif (HACKRF)

if (PLUTO)
  include_directories(
          pluto_handler_2
  )

  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          pluto_handler_2/pluto_handler.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          pluto_handler_2/pluto_handler.cpp
  )

  list(APPEND devices_ui forms/pluto_widget.ui)
  add_definitions(-DHAVE_PLUTO)
endif (PLUTO)

if (LIMESDR)
  include_directories(
          lime_handler
  )

  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          lime_handler/lime_handler.h
          lime_handler/lime_widget.h
          lime_handler/LimeSuite.h
          lime_handler/LMS7002M_parameters.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          lime_handler/lime_handler.cpp
  )

  add_definitions(-DHAVE_LIME)
endif (LIMESDR)

if (RTLSDR)
  include_directories(
          rtlsdr_handler
  )

  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          rtlsdr_handler/rtlsdr_handler.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          rtlsdr_handler/rtlsdr_handler.cpp
  )

  list(APPEND devices_ui forms/rtlsdr_widget.ui)
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
          uhd/uhd_handler.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          uhd/uhd_handler.cpp
  )

  list(APPEND devices_ui forms/uhd_widget.ui)
  add_definitions(-DHAVE_UHD)
endif ()

if (RTL_TCP)
  find_package(Qt6Network REQUIRED)
  include_directories(
          rtl_tcp
          ${Qt6Network_INCLUDE_DIRS}
  )

  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          rtl_tcp/rtl_tcp_client.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          rtl_tcp/rtl_tcp_client.cpp
  )

  list(APPEND devices_ui forms/rtl_tcp_widget.ui)
  set(RTLTCP_lib Qt6::Network)
  add_definitions(-DHAVE_RTL_TCP)
endif (RTL_TCP)

if (SOAPY)
  find_package(SoapySDR REQUIRED)
  include_directories(
          soapy/
  )

  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          soapy/soapy_handler.h
          soapy/soapy_worker.h
          soapy/soapy_converter.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          soapy/soapy_handler.cpp
          soapy/soapy_worker.cpp
          soapy/soapy_converter.cpp
  )

  list(APPEND devices_ui forms/soapy_handler.ui)
  set(LIBSOAPY "-lSoapySDR")
  list(APPEND extraLibs ${LIBSOAPY})
  add_definitions(-DHAVE_SOAPY)
endif ()

#---------------------------------------------------

include_directories(
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${QT6Widgets_INCLUDES}
        ${QT_QTCORE_INCLUDE_DIR}
        ${QT_QTGUI_INCLUDE_DIR}
        ${QT_QTCORE_INCLUDE_DIR}
        dummy-handler
        filereaders/filereader
        filereaders/xml_filereader
        filereaders/raw_files
        filereaders/wav_files
)


set(${devicesLibName}_HDRS
        ${${devicesLibName}_HDRS}
        ${CMAKE_SOURCE_DIR}/src/common/device_selector.h
        dongleselect.h
        filereaders/xml_filereader/xml_filereader.h
        filereaders/xml_filereader/xml_reader.h
        filereaders/xml_filereader/xml_descriptor.h
        filereaders/raw_files/rawfiles.h
        filereaders/raw_files/raw_reader.h
        filereaders/wav_files/wavfiles.h
        filereaders/wav_files/wav_reader.h
)

set(${devicesLibName}_SRCS
        ${${devicesLibName}_SRCS}
        device_selector.cpp
        dongleselect.cpp
        filereaders/xml_filereader/xml_filereader.cpp
        filereaders/xml_filereader/xml_reader.cpp
        filereaders/xml_filereader/xml_descriptor.cpp
        filereaders/raw_files/rawfiles.cpp
        filereaders/raw_files/raw_reader.cpp
        filereaders/wav_files/wavfiles.cpp
        filereaders/wav_files/wav_reader.cpp
)

qt6_wrap_ui(${devicesLibName}_UI_HDRS
        ${devices_ui}
)

set(${devicesLibName}_HDRS
        ${${devicesLibName}_HDRS}
        ${${devicesLibName}_UI_HDRS}
)

add_library(${devicesLibName} STATIC ${${devicesLibName}_SRCS} ${${devicesLibName}_HDRS})

target_link_libraries(${devicesLibName}
        PUBLIC
        ${objectName}_common
        PRIVATE
        Qt6::Widgets
        Qt6::Xml
        Qt6::Network
        ${extraLibs}
)

target_include_directories(${devicesLibName} INTERFACE .)
