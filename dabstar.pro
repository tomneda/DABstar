TARGET		= dabstar
TEMPLATE	= app
QT			+= widgets xml sql multimedia charts
CONFIG		+= console
#CONFIG		+= debug

# switch off to save compile/link time while development
CONFIG		+= use_lto 

use_lto	{
    DEFINES	        += QT_NO_DEBUG_OUTPUT
    QMAKE_CFLAGS	+= -flto
    QMAKE_CXXFLAGS	+= -flto
    QMAKE_LFLAGS	+= -flto
}

# math optimizations
QMAKE_CFLAGS	+= -ffast-math
QMAKE_CXXFLAGS	+= -Wall -Werror=return-type -ffast-math -fsingle-precision-constant
QMAKE_LFLAGS	+= -ffast-math

QMAKE_CXXFLAGS	+= -Wvla 

QMAKE_CXXFLAGS += -isystem $$[QT_INSTALL_HEADERS]
RC_ICONS	=  res/logo/dabstar.ico
RESOURCES	+= resources.qrc

DEFINES		+= APP_NAME=\\\"$$TARGET\\\"
DEFINES		+= PRJ_NAME=\\\"DABstar\\\"
DEFINES		+= PRJ_VERS=\\\"5.3.0\\\"

# For more parallel processing, uncomment the following
# defines
#DEFINES	+=  __THREADED_BACKEND

# For showing trace output
#DEFINES	+= __EPG_TRACE__

exists (".git") {
   GITHASHSTRING = $$system(git rev-parse --short HEAD)
   !isEmpty(GITHASHSTRING) {
       message("Current git hash = $$GITHASHSTRING")
       DEFINES += GITHASH=\\\"$$GITHASHSTRING\\\"
   }
}
isEmpty(GITHASHSTRING) {
    DEFINES += GITHASH=\\\"(unknown)\\\"
}

#DESTDIR 	= ../../DABstar-Qt6.11.1
LIBS		+= -L../../dabstar-libs/lib
CONFIG		+= airspy
CONFIG		+= spyServer
CONFIG		+= rtl_tcp
CONFIG		+= dabstick
CONFIG		+= sdrplay
CONFIG		+= pluto
CONFIG		+= hackrf
CONFIG		+= lime
CONFIG		+= soapy
CONFIG		+= sse2
#CONFIG		+= avx2
#CONFIG		+= fdk-aac
CONFIG		+= volk
#CONFIG		+= liquid
LIBS		+= -lsndfile.dll
LIBS		+= -lwinpthread
LIBS		+= -lws2_32
LIBS		+= -lz.dll
LIBS		+= -lfftw3f.dll
# very experimental, simple server for connecting to a tdc handler
#CONFIG		+= datastreamer

INCLUDEPATH += \
    ../dabstar-libs/include \
    src/main \
    src/ofdm \
    src/decoder \
    src/protection \
    src/backend \
    src/backend/audio \
    src/backend/data \
    src/backend/data/journaline \
    src/backend/data/mot \
    src/backend/data/epg_2 \
    src/backend/data/epg \
    src/support \
    src/support/tii_library \
    src/support/buttons \
    src/support/viterbi_spiral \
    src/audio \
    src/scopes \
    src/spectrum_viewer \
    src/file_devices/xml_filewriter \
    src/eti_handler \
    src/service_list \
    src/configuration \
    src/update \
    src/devices \
    src/devices/filereaders/filereader \
    src/devices/filereaders/xml_filereader \
    src/devices/filereaders/raw_files \
    src/devices/filereaders/wav_files \
    src/devices/dummy-handler \
    src/ensemble_list

HEADERS += \
    src/main/dabradio.h \
    src/main/audio_manager.h \
    src/main/epg_mot_handler.h \
    src/main/mot_slide_progress.h \
    src/main/tii_manager.h \
    src/main/gap_progress_bar.h \
    src/main/dab_channel_desc.h \
    src/main/glob_defs.h \
    src/main/glob_enums.h \
    src/main/glob_data_types.h \
    src/main/dab_processor.h \
    src/main/dab_constants.h \
    src/main/mot_content_types.h \
    src/eti_handler/eti_generator.h \
    src/main/bit_extractors.h \
    src/ofdm/sample_reader.h \
    src/ofdm/phasereference.h \
    src/ofdm/phasetable.h \
    src/ofdm/freq_interleaver.h \
    src/ofdm/tii_detector.h \
    src/ofdm/timesyncer.h \
    src/decoder/fib_decoder_if.h \
    src/decoder/fib_decoder.h \
    src/decoder/fib_config_fig0.h \
    src/decoder/fib_config_fig1.h \
    src/decoder/fib_table.h \
    src/decoder/fib_helper.h \
    src/decoder/fic_decoder.h \
    src/protection/protTables.h \
    src/protection/protection.h \
    src/protection/uep_protection.h \
    src/protection/eep_protection.h \
    src/backend/firecode_checker.h \
    src/backend/crc.h \
    src/backend/frame_processor.h \
    src/backend/charsets.h \
    src/backend/galois.h \
    src/backend/reed_solomon.h \
    src/backend/msc_handler.h \
    src/backend/backend.h \
    src/backend/backend_deconvolver.h \
    src/backend/backend_driver.h \
    src/backend/audio/mp4processor.h \
    src/backend/audio/bit_writer.h \
    src/backend/audio/mp2processor.h \
    src/backend/data/ip_datahandler.h \
    src/backend/data/tdc_datahandler.h \
    src/backend/data/journaline_datahandler.h \
    src/backend/data/journaline_viewer.h \
    src/backend/data/journaline/dabdatagroupdecoder.h \
    src/backend/data/journaline/crc_8_16.h \
    src/backend/data/journaline/log.h \
    src/backend/data/journaline/newssvcdec_impl.h \
    src/backend/data/journaline/Splitter.h \
    src/backend/data/journaline/dabdgdec_impl.h \
    src/backend/data/journaline/newsobject.h \
    src/backend/data/journaline/NML.h \
    src/backend/data/epg/epgdec.h \
    src/backend/data/epg_2/epg_decoder.h \
    src/backend/data/virtual_datahandler.h \
    src/backend/data/pad_handler.h \
    src/backend/data/mot/mot_handler.h \
    src/backend/data/mot/mot_object.h \
    src/backend/data/mot/mot_dir.h \
    src/backend/data/data_processor.h \
    src/audio/audiofifo.h \
    src/audio/audiooutput.h \
    src/audio/audiooutputqt.h \
    src/audio/audioiodevice.h \
    src/audio/test_tone.h \
    src/audio/delay_line.h \
    src/audio/resampler.h \
    src/support/fir_filters.h \
    src/support/ringbuffer.h \
    src/support/techdata.h \
    src/support/Xtan2.h \
    src/support/band_handler.h \
    src/support/dab_tables.h \
    src/support/tii_list_display.h \
    src/support/viterbi_spiral/viterbi_spiral.h \
    src/support/color_selector.h \
    src/support/time_table.h \
    src/support/openfiledialog.h \
    src/support/content_table.h \
    src/support/dl_cache.h \
    src/support/itu_regions.h \
    src/support/map_http_server.h \
    src/support/tii_library/tii_codes.h \
    #src/support/buttons/newpushbutton.h \
    #src/support/buttons/normalpushbutton.h \
    #src/support/buttons/circlepushbutton.h \
    src/support/custom_frame.h \
    src/support/gui_helpers.h \
    src/support/setting_helper.h \
    src/support/wav_writer.h \
    src/support/compass_direction.h \
    src/support/time_meas.h \
    src/support/copyright_info.h \
    src/scopes/iqdisplay.h \
    src/scopes/carrier_display.h \
    src/scopes/audio_display.h \
    src/scopes/plot_widget.h \
    src/scopes/level_meter.h \
    src/spectrum_viewer/spectrum_viewer.h \
    src/spectrum_viewer/spectrum_scope.h \
    src/spectrum_viewer/waterfall_scope.h \
    src/spectrum_viewer/correlation_viewer.h \
    src/spectrum_viewer/cir_viewer.h \
    src/file_devices/xml_filewriter/xml_filewriter.h \
    src/service_list/service_list_handler.h \
    src/service_list/service_db.h \
    src/configuration/configuration.h \
    src/ensemble_list/ensemble_list.h \
    src/ensemble_list/ensemble_list_db.h \
    src/ensemble_list/ensemble_list_db_handler.h \
    src/update/updatechecker.h \
    src/update/updatedialog.h \
    src/devices/device_handler.h \
    src/devices/device_exceptions.h \
    src/devices/device_selector.h \
    src/devices/dongleselect.h \
    src/devices/filereaders/xml_filereader/xml_filereader.h \
    src/devices/filereaders/xml_filereader/xml_reader.h \
    src/devices/filereaders/xml_filereader/xml_descriptor.h \
    src/devices/filereaders/raw_files/rawfiles.h \
    src/devices/filereaders/raw_files/raw_reader.h \
    src/devices/filereaders/wav_files/wavfiles.h \
    src/devices/filereaders/wav_files/wav_reader.h

SOURCES += \
    src/main/main.cpp \
    src/main/dabradio.cpp \
    src/main/dabradio_ui.cpp \
    src/main/dabradio_dump.cpp \
    src/main/audio_manager.cpp \
    src/main/epg_mot_handler.cpp \
    src/main/mot_slide_progress.cpp \
    src/main/tii_manager.cpp \
    src/main/dabradio_el.cpp \
    src/main/dabradio_ctrl.cpp \
    src/main/dab_channel_desc.cpp \
    src/main/dab_processor.cpp \
    src/support/techdata.cpp \
    src/eti_handler/eti_generator.cpp \
    src/ofdm/sample_reader.cpp \
    src/ofdm/phasereference.cpp \
    src/ofdm/phasetable.cpp \
    src/ofdm/freq_interleaver.cpp \
    src/ofdm/tii_detector.cpp \
    src/ofdm/timesyncer.cpp \
    src/decoder/fib_config_fig0.cpp \
    src/decoder/fib_config_fig1.cpp \
    src/decoder/fib_decoder.cpp \
    src/decoder/fib_decoder_fig0.cpp \
    src/decoder/fib_decoder_fig1.cpp \
    src/decoder/fib_decoder_string_getter.cpp \
    src/decoder/fib_helper.cpp \
    src/decoder/fic_decoder.cpp \
    src/protection/protTables.cpp \
    src/protection/protection.cpp \
    src/protection/eep_protection.cpp \
    src/protection/uep_protection.cpp \
    src/backend/firecode_checker.cpp \
    src/backend/crc.cpp \
    src/backend/charsets.cpp \
    src/backend/galois.cpp \
    src/backend/reed_solomon.cpp \
    src/backend/msc_handler.cpp \
    src/backend/backend.cpp \
    src/backend/backend_deconvolver.cpp \
    src/backend/backend_driver.cpp \
    src/backend/audio/mp4processor.cpp \
    src/backend/audio/bit_writer.cpp \
    src/backend/audio/mp2processor.cpp \
    src/backend/data/ip_datahandler.cpp \
    src/backend/data/journaline_datahandler.cpp \
    src/backend/data/journaline_viewer.cpp \
    src/backend/data/journaline/crc_8_16.c \
    src/backend/data/journaline/log.c \
    src/backend/data/journaline/newssvcdec_impl.cpp \
    src/backend/data/journaline/Splitter.cpp \
    src/backend/data/journaline/dabdgdec_impl.c \
    src/backend/data/journaline/newsobject.cpp \
    src/backend/data/journaline/NML.cpp \
    src/backend/data/epg_2/epg_decoder.cpp \
    src/backend/data/epg/epgdec.cpp \
    src/backend/data/tdc_datahandler.cpp \
    src/backend/data/pad_handler.cpp \
    src/backend/data/mot/mot_handler.cpp \
    src/backend/data/mot/mot_object.cpp \
    src/backend/data/mot/mot_dir.cpp \
    src/backend/data/data_processor.cpp \
    src/audio/audioiodevice.cpp \
    src/audio/test_tone.cpp \
    src/audio/audiooutputqt.cpp \
    src/support/fir_filters.cpp \
    src/support/ringbuffer.cpp \
    src/support/Xtan2.cpp \
    src/support/band_handler.cpp \
    src/support/dab_tables.cpp \
    #src/support/buttons/newpushbutton.cpp \
    #src/support/buttons/normalpushbutton.cpp \
    #src/support/buttons/circlepushbutton.cpp \
    src/support/viterbi_spiral/viterbi_spiral.cpp \
    src/support/color_selector.cpp \
    src/support/time_table.cpp \
    src/support/openfiledialog.cpp \
    src/support/content_table.cpp \
    src/support/dl_cache.cpp \
    src/support/itu_regions.cpp \
    src/support/map_http_server.cpp \
    src/support/tii_list_display.cpp \
    src/support/tii_library/tii_codes.cpp \
    src/support/custom_frame.cpp \
    src/support/gui_helpers.cpp \
    src/support/setting_helper.cpp \
    src/support/wav_writer.cpp \
    src/support/compass_direction.cpp \
    src/support/copyright_info.cpp \
    src/scopes/iqdisplay.cpp \
    src/scopes/carrier_display.cpp \
    src/scopes/audio_display.cpp \
    src/scopes/plot_widget.cpp \
    src/scopes/level_meter.cpp \
    src/spectrum_viewer/spectrum_viewer.cpp \
    src/spectrum_viewer/spectrum_scope.cpp \
    src/spectrum_viewer/waterfall_scope.cpp \
    src/spectrum_viewer/correlation_viewer.cpp \
    src/spectrum_viewer/cir_viewer.cpp \
    src/file_devices/xml_filewriter/xml_filewriter.cpp \
    src/service_list/service_list_handler.cpp \
    src/service_list/service_db.cpp \
    src/configuration/configuration.cpp \
    src/ensemble_list/ensemble_list.cpp \
    src/ensemble_list/ensemble_list_db.cpp \
    src/ensemble_list/ensemble_list_db_handler.cpp \
    src/update/updatechecker.cpp \
    src/update/updatedialog.cpp \
    src/devices/device_selector.cpp \
    src/devices/dongleselect.cpp \
    src/devices/filereaders/xml_filereader/xml_filereader.cpp \
    src/devices/filereaders/xml_filereader/xml_reader.cpp \
    src/devices/filereaders/xml_filereader/xml_descriptor.cpp \
    src/devices/filereaders/raw_files/rawfiles.cpp \
    src/devices/filereaders/raw_files/raw_reader.cpp \
    src/devices/filereaders/wav_files/wavfiles.cpp \
    src/devices/filereaders/wav_files/wav_reader.cpp

FORMS += \
    forms/cir_widget.ui \
    forms/configuration.ui \
    forms/dabradio.ui \
    forms/dumpwidget.ui \
    forms/ensemble_list.ui \
    forms/spectrum_viewer.ui \
    forms/techdata.ui \
    forms/updatedialog.ui \
    src/devices/forms/xmlfiles.ui

#	dabstick
#	Note: the windows version is bound to the dll, the
#	linux version loads the function from the so
dabstick {
    DEFINES		+= HAVE_RTLSDR
    INCLUDEPATH	+= src/devices/rtlsdr_handler
    HEADERS		+= src/devices/rtlsdr_handler/rtlsdr_handler.h
    SOURCES		+= src/devices/rtlsdr_handler/rtlsdr_handler.cpp
    FORMS		+= src/devices/forms/rtlsdr_widget.ui
}

#
#	the SDRplay
#
sdrplay {
    DEFINES		+= HAVE_SDRPLAY
    INCLUDEPATH	+= src/devices/sdrplay_handler \
               ../dabstar-libs/include/sdrplay
    HEADERS		+= src/devices/sdrplay_handler/sdrplay_handler.h \
               src/devices/sdrplay_handler/sdrplay_commands.h \
               src/devices/sdrplay_handler/Rsp_device.h \
               src/devices/sdrplay_handler/Rsp1_handler.h \
               src/devices/sdrplay_handler/Rsp1A_handler.h \
               src/devices/sdrplay_handler/Rsp2_handler.h \
               src/devices/sdrplay_handler/RspDuo_handler.h \
               src/devices/sdrplay_handler/RspDx_handler.h
    SOURCES		+= src/devices/sdrplay_handler/Rsp_device.cpp \
               src/devices/sdrplay_handler/sdrplay_handler.cpp \
               src/devices/sdrplay_handler/Rsp1_handler.cpp \
               src/devices/sdrplay_handler/Rsp1A_handler.cpp \
               src/devices/sdrplay_handler/Rsp2_handler.cpp \
               src/devices/sdrplay_handler/RspDuo_handler.cpp \
               src/devices/sdrplay_handler/RspDx_handler.cpp
    FORMS		+= src/devices/forms/sdrplay_widget.ui
}

#
#	limeSDR
#
lime  {
    DEFINES		+= HAVE_LIME
    INCLUDEPATH	+= src/devices/lime_handler
    HEADERS		+= src/devices/lime_handler/lime_handler.h \
               src/devices/lime_handler/lime_widget.h
    SOURCES		+= src/devices/lime_handler/lime_handler.cpp
}

#
#	the hackrf
#
hackrf {
    DEFINES		+= HAVE_HACKRF
    INCLUDEPATH	+= src/devices/hackrf_handler
    HEADERS		+= src/devices/hackrf_handler/hackrf_handler.h
    SOURCES		+= src/devices/hackrf_handler/hackrf_handler.cpp
    FORMS		+= src/devices/forms/hackrf_widget.ui
}

#
# airspy support
#
airspy {
    DEFINES		+= HAVE_AIRSPY
    INCLUDEPATH	+= src/devices/airspy_handler
    HEADERS		+= src/devices/airspy_handler/airspy_handler.h
    SOURCES		+= src/devices/airspy_handler/airspy_handler.cpp
    FORMS		+= src/devices/forms/airspy_widget.ui
}

#	extio dependencies, windows only
#
extio {
    DEFINES		+= HAVE_EXTIO
    INCLUDEPATH	+= src/devices/extio_handler
    HEADERS		+= src/devices/extio_handler/extio_handler.h \
               src/devices/extio_handler/common_readers.h \
               src/devices/extio_handler/virtual_reader.h
    SOURCES		+= src/devices/extio_handler/extio_handler.cpp \
               src/devices/extio_handler/common_readers.cpp \
               src/devices/extio_handler/virtual_reader.cpp
}

#
rtl_tcp {
    DEFINES		+= HAVE_RTL_TCP
    QT		+= network
    INCLUDEPATH	+= src/devices/rtl_tcp
    HEADERS		+= src/devices/rtl_tcp/rtl_tcp_client.h
    SOURCES		+= src/devices/rtl_tcp/rtl_tcp_client.cpp
    FORMS		+= src/devices/forms/rtl_tcp_widget.ui
}

soapy {
    DEFINES		+= HAVE_SOAPY
    INCLUDEPATH	+= src/devices/soapy
    HEADERS		+= src/devices/soapy/soapy_handler.h \
               src/devices/soapy/soapy_worker.h \
               src/devices/soapy/soapy_converter.h
    SOURCES		+= src/devices/soapy/soapy_handler.cpp \
               src/devices/soapy/soapy_worker.cpp \
               src/devices/soapy/soapy_converter.cpp
    FORMS		+= src/devices/forms/soapy_handler.ui
    LIBS		+= -lSoapySDR
}

pluto	{
    DEFINES		+= HAVE_PLUTO
    QT		+= network
    INCLUDEPATH	+= src/devices/pluto_handler_2
    HEADERS		+= src/devices/pluto_handler_2/dabFilter.h
    HEADERS		+= src/devices/pluto_handler_2/pluto_handler.h
    SOURCES		+= src/devices/pluto_handler_2/pluto_handler.cpp
    FORMS		+= src/devices/forms/pluto_widget.ui
}

elad-device	{
    DEFINES		+= HAVE_ELAD
    INCLUDEPATH	+= src/devices/elad_s1_handler
    HEADERS		+= src/devices/elad_s1_handler/elad_handler.h
    HEADERS		+= src/devices/elad_s1_handler/elad_loader.h
    HEADERS		+= src/devices/elad_s1_handler/elad_worker.h
    SOURCES		+= src/devices/elad_s1_handler/elad_handler.cpp
    SOURCES		+= src/devices/elad_s1_handler/elad_loader.cpp
    SOURCES		+= src/devices/elad_s1_handler/elad_worker.cpp
    FORMS		+= src/devices/forms/elad_widget.ui
}

spyServer  {
    DEFINES		+= HAVE_SPYSERVER
    INCLUDEPATH	+= src/devices/spy_server
    HEADERS		+= src/devices/spy_server/spyserver_protocol.h \
               src/devices/spy_server/spyserver_tcp_client.h \
               src/devices/spy_server/spyserver_handler.h \
               src/devices/spy_server/spyserver_client.h
    SOURCES		+= src/devices/spy_server/spyserver_tcp_client.cpp \
               src/devices/spy_server/spyserver_handler.cpp \
               src/devices/spy_server/spyserver_client.cpp
    FORMS		+= src/devices/forms/spyserver_client.ui
}

uhd	{
    DEFINES		+= HAVE_UHD
    INCLUDEPATH	+= src/devices/uhd
    HEADERS		+= src/devices/uhd/uhd_handler.h
    SOURCES		+= src/devices/uhd/uhd_handler.cpp
    FORMS		+= src/devices/forms/uhd_widget.ui
    LIBS		+= -luhd
}

colibri	{
    DEFINES		+= HAVE_COLIBRI
    INCLUDEPATH	+= src/devices/colibri_handler
    HEADERS		+= src/devices/colibri_handler/common.h
    HEADERS		+= src/devices/colibri_handler/LibLoader.h
    HEADERS		+= src/devices/colibri_handler/colibri_handler.h
    SOURCES		+= src/devices/colibri_handler/LibLoader.cpp
    SOURCES		+= src/devices/colibri_handler/colibri_handler.cpp
    FORMS		+= src/devices/forms/colibri_widget.ui
}

datastreamer	{
    DEFINES		+= DATA_STREAMER
    DEFINES		+= CLOCK_STREAMER
    INCLUDEPATH	+= src/server_thread
    HEADERS		+= src/server_thread/tcp_server.h
    SOURCES		+= src/server_thread/tcp_server.cpp
}

avx2	{
    QMAKE_CXXFLAGS	+= -mavx2
    DEFINES		+= HAVE_VITERBI_AVX2
    HEADERS		+= src/support/viterbi_spiral/viterbi_16way.h
}else:sse2	{
    DEFINES		+= HAVE_VITERBI_SSE2
    HEADERS		+= src/support/viterbi_spiral/viterbi_8way.h
}else	{
    HEADERS		+= src/support/viterbi_spiral/viterbi_scalar.h
}


fdk-aac {
    DEFINES		+= __WITH_FDK_AAC__
    INCLUDEPATH	+= ../dabstar-libs/include/fdk-aac
    HEADERS		+= src/backend/audio/fdk_aac.h
    SOURCES		+= src/backend/audio/fdk_aac.cpp
    LIBS		+= -lfdk-aac.dll
}else	{
    HEADERS		+= src/backend/audio/faad_decoder.h
    SOURCES		+= src/backend/audio/faad_decoder.cpp
    LIBS		+= -lfaad.dll
}

volk	{
    DEFINES		+= HAVE_SSE_OR_AVX
    HEADERS		+= src/support/simd_extensions.h \
               src/ofdm/ofdm_decoder_simd.h
    SOURCES		+= src/ofdm/ofdm_decoder_simd.cpp
    LIBS		+= -lvolk.dll
}else	{
    HEADERS		+= src/ofdm/ofdm_decoder.h
    SOURCES		+= src/ofdm/ofdm_decoder.cpp
}

liquid	{
    DEFINES		+= HAVE_LIQUID
    HEADERS		+= src/support/halfbandfilter.h
    SOURCES		+= src/support/halfbandfilter.cpp
    LIBS		+= -lliquid.dll
}
