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
DEFINES		+= PRJ_VERS=\\\"5.4.0\\\"

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
CONFIG		+= fdk-aac
CONFIG		+= volk
CONFIG		+= liquid
LIBS		+= -lsndfile.dll
LIBS		+= -lwinpthread
LIBS		+= -lws2_32
LIBS		+= -lz.dll
LIBS		+= -lfftw3f.dll
# very experimental, simple server for connecting to a tdc handler
#CONFIG		+= datastreamer

# --- external ---
# Bundled third-party headers (liquid-dsp, ...) we do not control. Use -isystem
# instead of INCLUDEPATH (-I) so the compiler suppresses their warnings (e.g.
# liquid's -Wdeprecated-declarations) while still fully warning on our own code.
# Matches the CMake include_directories(SYSTEM ...) treatment.
EXT_INC = ../../dabstar-libs/include
QMAKE_CXXFLAGS += -isystem $$EXT_INC
QMAKE_CFLAGS   += -isystem $$EXT_INC

# --- src/common ---
INCLUDEPATH += \
    src/common

# --- src/base ---
INCLUDEPATH += \
    src/base/audio \
    src/base/backend \
    src/base/backend/audio \
    src/base/backend/data \
    src/base/backend/data/epg \
    src/base/backend/data/epg_2 \
    src/base/backend/data/journaline \
    src/base/backend/data/mot \
    src/base/configuration \
    src/base/decoder \
    src/base/ensemble_list \
    src/base/eti_handler \
    src/base/main \
    src/base/ofdm \
    src/base/protection \
    src/base/scopes \
    src/base/server_thread \
    src/base/service_list \
    src/base/spectrum_viewer \
    src/base/support \
    src/base/support/buttons \
    src/base/support/tii_library \
    src/base/support/viterbi_spiral \
    src/base/update

# --- src/devices ---
INCLUDEPATH += \
    src/devices \
    src/devices/filereaders/filereader \
    src/devices/filereaders/raw_files \
    src/devices/filereaders/wav_files \
    src/devices/filereaders/xml_filereader

# --- src/common ---
HEADERS += \
    src/common/dab_constants.h \
    src/common/device_handler_if.h \
    src/common/device_selector_if.h \
    src/common/fir_filters.h \
    src/common/glob_data_types.h \
    src/common/glob_defs.h \
    src/common/openfiledialog.h \
    src/common/ringbuffer.h \
    src/common/setting_helper.cnf.h \
    src/common/setting_helper.h \
    src/common/xml_filewriter.h

# --- src/base ---
HEADERS += \
    src/base/audio/audiofifo.h \
    src/base/audio/audioiodevice.h \
    src/base/audio/audiooutput_if.h \
    src/base/audio/audiooutputqt.h \
    src/base/audio/delay_line.h \
    src/base/audio/resampler.h \
    src/base/audio/test_tone.h \
    src/base/backend/backend.h \
    src/base/backend/backend_deconvolver.h \
    src/base/backend/backend_driver.h \
    src/base/backend/charsets.h \
    src/base/backend/crc.h \
    src/base/backend/firecode_checker.h \
    src/base/backend/frame_processor.h \
    src/base/backend/galois.h \
    src/base/backend/mm_malloc.h \
    src/base/backend/msc_handler.h \
    src/base/backend/reed_solomon.h \
    src/base/backend/audio/bit_writer.h \
    src/base/backend/audio/mp2processor.h \
    src/base/backend/audio/mp4processor.h \
    src/base/backend/data/data_processor.h \
    src/base/backend/data/ip_datahandler.h \
    src/base/backend/data/journaline_datahandler.h \
    src/base/backend/data/journaline_viewer.h \
    src/base/backend/data/pad_handler.h \
    src/base/backend/data/tdc_datahandler.h \
    src/base/backend/data/virtual_datahandler.h \
    src/base/backend/data/epg/epgdec.h \
    src/base/backend/data/epg_2/epg_decoder.h \
    src/base/backend/data/journaline/cpplog.h \
    src/base/backend/data/journaline/crc_8_16.h \
    src/base/backend/data/journaline/dabdatagroupdecoder.h \
    src/base/backend/data/journaline/dabdgdec_impl.h \
    src/base/backend/data/journaline/log.h \
    src/base/backend/data/journaline/newsobject.h \
    src/base/backend/data/journaline/newssvcdec.h \
    src/base/backend/data/journaline/newssvcdec_impl.h \
    src/base/backend/data/journaline/NML.h \
    src/base/backend/data/journaline/Splitter.h \
    src/base/backend/data/mot/mot_dir.h \
    src/base/backend/data/mot/mot_handler.h \
    src/base/backend/data/mot/mot_object.h \
    src/base/configuration/configuration.h \
    src/base/decoder/fib_config_fig0.h \
    src/base/decoder/fib_config_fig1.h \
    src/base/decoder/fib_decoder.h \
    src/base/decoder/fib_decoder_if.h \
    src/base/decoder/fib_helper.h \
    src/base/decoder/fib_table.h \
    src/base/decoder/fic_decoder.h \
    src/base/ensemble_list/ensemble_list.h \
    src/base/ensemble_list/ensemble_list_db.h \
    src/base/ensemble_list/ensemble_list_db_handler.h \
    src/base/eti_handler/eti_generator.h \
    src/base/main/audio_manager.h \
    src/base/main/bit_extractors.h \
    src/base/main/dab_channel_desc.h \
    src/base/main/dab_processor.h \
    src/base/main/dabradio.h \
    src/base/main/epg_mot_handler.h \
    src/base/main/gap_progress_bar.h \
    src/base/main/glob_enums.h \
    src/base/main/mot_content_types.h \
    src/base/main/mot_slide_progress.h \
    src/base/main/tii_manager.h \
    src/base/ofdm/freq_interleaver.h \
    src/base/ofdm/phasereference.h \
    src/base/ofdm/phasetable.h \
    src/base/ofdm/sample_reader.h \
    src/base/ofdm/tii_detector.h \
    src/base/ofdm/timesyncer.h \
    src/base/protection/eep_protection.h \
    src/base/protection/protection.h \
    src/base/protection/protTables.h \
    src/base/protection/uep_protection.h \
    src/base/scopes/audio_display.h \
    src/base/scopes/carrier_display.h \
    src/base/scopes/iqdisplay.h \
    src/base/scopes/level_meter.h \
    src/base/scopes/plot_widget.h \
    src/base/service_list/service_db.h \
    src/base/service_list/service_list_handler.h \
    src/base/spectrum_viewer/cir_viewer.h \
    src/base/spectrum_viewer/correlation_viewer.h \
    src/base/spectrum_viewer/spectrum_scope.h \
    src/base/spectrum_viewer/spectrum_viewer.h \
    src/base/spectrum_viewer/waterfall_scope.h \
    src/base/support/band_handler.h \
    src/base/support/color_selector.h \
    src/base/support/compass_direction.h \
    src/base/support/content_table.h \
    src/base/support/converted_map.h \
    src/base/support/copyright_info.h \
    src/base/support/custom_frame.h \
    src/base/support/dab_tables.h \
    src/base/support/dl_cache.h \
    src/base/support/gui_helpers.h \
    src/base/support/itu_regions.h \
    src/base/support/map_http_server.h \
    src/base/support/plotter.h \
    src/base/support/process_params.h \
    src/base/support/techdata.h \
    src/base/support/tii_list_display.h \
    src/base/support/time_meas.h \
    src/base/support/time_table.h \
    src/base/support/wav_writer.h \
    src/base/support/tii_library/tii_codes.h \
    src/base/support/viterbi_spiral/viterbi_spiral.h \
    src/base/update/appversion.h \
    src/base/update/updatechecker.h \
    src/base/update/updatedialog.h

# --- src/devices ---
HEADERS += \
    src/devices/device_exceptions.h \
    src/devices/device_selector.h \
    src/devices/dongleselect.h \
    src/devices/filereaders/raw_files/raw_reader.h \
    src/devices/filereaders/raw_files/rawfiles.h \
    src/devices/filereaders/wav_files/wav_reader.h \
    src/devices/filereaders/wav_files/wavfiles.h \
    src/devices/filereaders/xml_filereader/xml_descriptor.h \
    src/devices/filereaders/xml_filereader/xml_filereader.h \
    src/devices/filereaders/xml_filereader/xml_reader.h

# --- src/common ---
SOURCES += \
    src/common/fir_filters.cpp \
    src/common/openfiledialog.cpp \
    src/common/setting_helper.cpp \
    src/common/xml_filewriter.cpp

# --- src/base ---
SOURCES += \
    src/base/audio/audioiodevice.cpp \
    src/base/audio/audiooutputqt.cpp \
    src/base/audio/test_tone.cpp \
    src/base/backend/backend.cpp \
    src/base/backend/backend_deconvolver.cpp \
    src/base/backend/backend_driver.cpp \
    src/base/backend/charsets.cpp \
    src/base/backend/crc.cpp \
    src/base/backend/firecode_checker.cpp \
    src/base/backend/galois.cpp \
    src/base/backend/msc_handler.cpp \
    src/base/backend/reed_solomon.cpp \
    src/base/backend/audio/bit_writer.cpp \
    src/base/backend/audio/mp2processor.cpp \
    src/base/backend/audio/mp4processor.cpp \
    src/base/backend/data/data_processor.cpp \
    src/base/backend/data/ip_datahandler.cpp \
    src/base/backend/data/journaline_datahandler.cpp \
    src/base/backend/data/journaline_viewer.cpp \
    src/base/backend/data/pad_handler.cpp \
    src/base/backend/data/tdc_datahandler.cpp \
    src/base/backend/data/epg/epgdec.cpp \
    src/base/backend/data/epg_2/epg_decoder.cpp \
    src/base/backend/data/journaline/crc_8_16.c \
    src/base/backend/data/journaline/dabdgdec_impl.c \
    src/base/backend/data/journaline/log.c \
    src/base/backend/data/journaline/newsobject.cpp \
    src/base/backend/data/journaline/newssvcdec_impl.cpp \
    src/base/backend/data/journaline/NML.cpp \
    src/base/backend/data/journaline/Splitter.cpp \
    src/base/backend/data/mot/mot_dir.cpp \
    src/base/backend/data/mot/mot_handler.cpp \
    src/base/backend/data/mot/mot_object.cpp \
    src/base/configuration/configuration.cpp \
    src/base/decoder/fib_config_fig0.cpp \
    src/base/decoder/fib_config_fig1.cpp \
    src/base/decoder/fib_decoder.cpp \
    src/base/decoder/fib_decoder_fig0.cpp \
    src/base/decoder/fib_decoder_fig1.cpp \
    src/base/decoder/fib_decoder_string_getter.cpp \
    src/base/decoder/fib_helper.cpp \
    src/base/decoder/fic_decoder.cpp \
    src/base/ensemble_list/ensemble_list.cpp \
    src/base/ensemble_list/ensemble_list_db.cpp \
    src/base/ensemble_list/ensemble_list_db_handler.cpp \
    src/base/eti_handler/eti_generator.cpp \
    src/base/main/audio_manager.cpp \
    src/base/main/dab_channel_desc.cpp \
    src/base/main/dab_processor.cpp \
    src/base/main/dabradio.cpp \
    src/base/main/dabradio_ctrl.cpp \
    src/base/main/dabradio_dump.cpp \
    src/base/main/dabradio_el.cpp \
    src/base/main/dabradio_ui.cpp \
    src/base/main/epg_mot_handler.cpp \
    src/base/main/main.cpp \
    src/base/main/mot_slide_progress.cpp \
    src/base/main/tii_manager.cpp \
    src/base/ofdm/freq_interleaver.cpp \
    src/base/ofdm/phasereference.cpp \
    src/base/ofdm/phasetable.cpp \
    src/base/ofdm/sample_reader.cpp \
    src/base/ofdm/tii_detector.cpp \
    src/base/ofdm/timesyncer.cpp \
    src/base/protection/eep_protection.cpp \
    src/base/protection/protection.cpp \
    src/base/protection/protTables.cpp \
    src/base/protection/uep_protection.cpp \
    src/base/scopes/audio_display.cpp \
    src/base/scopes/carrier_display.cpp \
    src/base/scopes/iqdisplay.cpp \
    src/base/scopes/level_meter.cpp \
    src/base/scopes/plot_widget.cpp \
    src/base/service_list/service_db.cpp \
    src/base/service_list/service_list_handler.cpp \
    src/base/spectrum_viewer/cir_viewer.cpp \
    src/base/spectrum_viewer/correlation_viewer.cpp \
    src/base/spectrum_viewer/spectrum_scope.cpp \
    src/base/spectrum_viewer/spectrum_viewer.cpp \
    src/base/spectrum_viewer/waterfall_scope.cpp \
    src/base/support/band_handler.cpp \
    src/base/support/color_selector.cpp \
    src/base/support/compass_direction.cpp \
    src/base/support/content_table.cpp \
    src/base/support/copyright_info.cpp \
    src/base/support/custom_frame.cpp \
    src/base/support/dab_tables.cpp \
    src/base/support/dl_cache.cpp \
    src/base/support/gui_helpers.cpp \
    src/base/support/itu_regions.cpp \
    src/base/support/map_http_server.cpp \
    src/base/support/ringbuffer.cpp \
    src/base/support/techdata.cpp \
    src/base/support/tii_list_display.cpp \
    src/base/support/time_table.cpp \
    src/base/support/wav_writer.cpp \
    src/base/support/tii_library/tii_codes.cpp \
    src/base/support/viterbi_spiral/viterbi_spiral.cpp \
    src/base/update/updatechecker.cpp \
    src/base/update/updatedialog.cpp

# --- src/devices ---
SOURCES += \
    src/devices/device_selector.cpp \
    src/devices/dongleselect.cpp \
    src/devices/filereaders/raw_files/raw_reader.cpp \
    src/devices/filereaders/raw_files/rawfiles.cpp \
    src/devices/filereaders/wav_files/wav_reader.cpp \
    src/devices/filereaders/wav_files/wavfiles.cpp \
    src/devices/filereaders/xml_filereader/xml_descriptor.cpp \
    src/devices/filereaders/xml_filereader/xml_filereader.cpp \
    src/devices/filereaders/xml_filereader/xml_reader.cpp

# --- src/base/forms ---
FORMS += \
    src/base/forms/cir_widget.ui \
    src/base/forms/configuration.ui \
    src/base/forms/dabradio.ui \
    src/base/forms/dumpwidget.ui \
    src/base/forms/ensemble_list.ui \
    src/base/forms/spectrum_viewer.ui \
    src/base/forms/techdata.ui \
    src/base/forms/updatedialog.ui

# --- src/devices/forms ---
FORMS += \
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
               $$EXT_INC/sdrplay
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
               src/devices/lime_handler/lime_widget.h \
               src/devices/lime_handler/LimeSuite.h \
               src/devices/lime_handler/LMS7002M_parameters.h
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
    HEADERS		+= src/devices/pluto_handler_2/dabFilter.h \
               src/devices/pluto_handler_2/pluto_handler.h
    SOURCES		+= src/devices/pluto_handler_2/pluto_handler.cpp
    FORMS		+= src/devices/forms/pluto_widget.ui
}

elad-device	{
    DEFINES		+= HAVE_ELAD
    INCLUDEPATH	+= src/devices/elad_s1_handler
    HEADERS		+= src/devices/elad_s1_handler/elad_handler.h \
               src/devices/elad_s1_handler/elad_loader.h \
               src/devices/elad_s1_handler/elad_worker.h
    SOURCES		+= src/devices/elad_s1_handler/elad_handler.cpp \
               src/devices/elad_s1_handler/elad_loader.cpp \
               src/devices/elad_s1_handler/elad_worker.cpp
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
    HEADERS		+= src/devices/colibri_handler/common.h \
               src/devices/colibri_handler/LibLoader.h \
               src/devices/colibri_handler/colibri_handler.h
    SOURCES		+= src/devices/colibri_handler/LibLoader.cpp \
               src/devices/colibri_handler/colibri_handler.cpp
    FORMS		+= src/devices/forms/colibri_widget.ui
}

datastreamer	{
    DEFINES		+= DATA_STREAMER
    DEFINES		+= CLOCK_STREAMER
    INCLUDEPATH	+= src/base/server_thread
    HEADERS		+= src/base/server_thread/tcp_server.h
    SOURCES		+= src/base/server_thread/tcp_server.cpp
}

avx2	{
    QMAKE_CXXFLAGS	+= -mavx2
    DEFINES		+= HAVE_VITERBI_AVX2
    HEADERS		+= src/base/support/viterbi_spiral/viterbi_16way.h
}else:sse2	{
    DEFINES		+= HAVE_VITERBI_SSE2
    HEADERS		+= src/base/support/viterbi_spiral/viterbi_8way.h
}else	{
    HEADERS		+= src/base/support/viterbi_spiral/viterbi_scalar.h
}


fdk-aac {
    DEFINES		+= __WITH_FDK_AAC__
    INCLUDEPATH	+= $$EXT_INC/fdk-aac
    HEADERS		+= src/base/backend/audio/fdk_aac.h
    SOURCES		+= src/base/backend/audio/fdk_aac.cpp
    LIBS		+= -lfdk-aac.dll
}else	{
    HEADERS		+= src/base/backend/audio/faad_decoder.h
    SOURCES		+= src/base/backend/audio/faad_decoder.cpp
    LIBS		+= -lfaad.dll
}

volk	{
    DEFINES		+= HAVE_SSE_OR_AVX
    HEADERS		+= src/base/support/simd_extensions.h \
               src/base/ofdm/ofdm_decoder_simd.h
    SOURCES		+= src/base/ofdm/ofdm_decoder_simd.cpp
    LIBS		+= -lvolk.dll
}else	{
    HEADERS		+= src/base/ofdm/ofdm_decoder.h
    SOURCES		+= src/base/ofdm/ofdm_decoder.cpp
}

liquid	{
    DEFINES		+= HAVE_LIQUID
    HEADERS		+= src/common/halfbandfilter.h
    SOURCES		+= src/common/halfbandfilter.cpp
    LIBS		+= -lliquid.dll
}
