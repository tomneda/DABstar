
set(devicesLibName ${objectName}_devices)

# UI files with no filtered preselection
set(devices_ui
        forms/colibri_widget.ui
        forms/elad_files.ui
        forms/elad_widget.ui
        forms/extio_widget.ui
        forms/xmlfiles.ui
)

if (SDRPLAY)
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
  list(APPEND devicesExtraIncludes sdrplay_handler)
  add_definitions(-DHAVE_SDRPLAY)
endif ()

if (AIRSPY)
  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          airspy_handler/airspy_handler.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          airspy_handler/airspy_handler.cpp
  )

  list(APPEND devices_ui forms/airspy_widget.ui)
  list(APPEND devicesExtraIncludes airspy_handler)
  add_definitions(-DHAVE_AIRSPY)
endif ()

if (SPYSERVER)
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
  list(APPEND devicesExtraIncludes spy_server)
  add_definitions(-DHAVE_SPYSERVER)
endif ()

if (HACKRF)
  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          hackrf_handler/hackrf_handler.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          hackrf_handler/hackrf_handler.cpp
  )

  list(APPEND devices_ui forms/hackrf_widget.ui)
  list(APPEND devicesExtraIncludes hackrf_handler)
  add_definitions(-DHAVE_HACKRF)
endif ()

if (PLUTO)
  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          pluto_handler_2/pluto_handler.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          pluto_handler_2/pluto_handler.cpp
  )

  list(APPEND devices_ui forms/pluto_widget.ui)
  list(APPEND devicesExtraIncludes pluto_handler_2)
  add_definitions(-DHAVE_PLUTO)
endif ()

if (LIMESDR)
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

  list(APPEND devicesExtraIncludes lime_handler)
  add_definitions(-DHAVE_LIME)
endif ()

if (RTLSDR)
  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          rtlsdr_handler/rtlsdr_handler.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          rtlsdr_handler/rtlsdr_handler.cpp
  )

  list(APPEND devices_ui forms/rtlsdr_widget.ui)
  list(APPEND devicesExtraIncludes rtlsdr_handler)
  add_definitions(-DHAVE_RTLSDR)
endif ()

if (UHD)
  find_package(UHD REQUIRED)
  list(APPEND devicesExtraLibs ${UHD_LIBRARIES})

  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          uhd/uhd_handler.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          uhd/uhd_handler.cpp
  )

  list(APPEND devices_ui forms/uhd_widget.ui)
  list(APPEND devicesExtraIncludes uhd)
  add_definitions(-DHAVE_UHD)
endif ()

if (RTL_TCP)
  set(${devicesLibName}_HDRS
          ${${devicesLibName}_HDRS}
          rtl_tcp/rtl_tcp_client.h
  )

  set(${devicesLibName}_SRCS
          ${${devicesLibName}_SRCS}
          rtl_tcp/rtl_tcp_client.cpp
  )

  list(APPEND devices_ui forms/rtl_tcp_widget.ui)
  list(APPEND devicesExtraIncludes rtl_tcp)
  add_definitions(-DHAVE_RTL_TCP)
endif ()

if (SOAPY)
  find_package(SoapySDR REQUIRED)

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
  list(APPEND devicesExtraLibs ${SoapySDR_LIBRARIES})
  list(APPEND devicesExtraIncludes soapy)
  add_definitions(-DHAVE_SOAPY)
endif ()

#---------------------------------------------------

set(${devicesLibName}_HDRS
        ${${devicesLibName}_HDRS}
        device_selector.h
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

target_include_directories(${devicesLibName}
        PUBLIC
        .
        PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        dummy-handler
        filereaders/filereader
        filereaders/xml_filereader
        filereaders/raw_files
        filereaders/wav_files
        ${devicesExtraIncludes}
)

target_link_libraries(${devicesLibName}
        PUBLIC
        ${objectName}_common
        PRIVATE
        Qt6::Widgets
        Qt6::Xml
        Qt6::Network
        ${devicesExtraLibs}
)
