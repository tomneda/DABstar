#########################################################################
# Automatically generated by qmake (2.01a) Tue Oct 6 19:48:14 2009
# but modified by me to accomodate for the includes for qwt and portaudio
#########################################################################

TARGET		= dabstar
TEMPLATE	= app
QT		+= widgets xml sql multimedia
CONFIG		+= console
#CONFIG		-= console
CONFIG		+= release
#CONFIG		+= debug
QMAKE_CXXFLAGS	+= -std=c++17

QMAKE_CFLAGS	+= -O3 -ffast-math -flto
QMAKE_CXXFLAGS	+= -O3 -ffast-math -flto
QMAKE_LFLAGS	+= -O3 -ffast-math -flto
QMAKE_LFLAGS    += -mthreads

QMAKE_CXXFLAGS += -isystem $$[QT_INSTALL_HEADERS]
RC_ICONS	=  res/icons/qt-dab-6.ico
RESOURCES	+= resources.qrc

DEFINES		+= APP_NAME=\\\"$$TARGET\\\"
DEFINES		+= PRJ_NAME=\\\"DABstar\\\"
DEFINES		+= PRJ_VERS=\\\"3.2.0\\\"

# For more parallel processing, uncomment the following
# defines
#DEFINES	+=  __THREADED_BACKEND

DEFINES		+= __BITS64__
DEFINES		+= __FFTW3__
#DEFINES	+= __KISS_FFT__

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


DEPENDPATH += src \
    src/fft \
    src/main \
    src/ofdm \
    src/protection \
    src/backend \
    src/backend/audio \
    src/backend/data \
    src/backend/data/journaline \
    src/backend/data/mot \
    src/backend/data/epg-2 \
    src/backend/data/epg \
    src/support \
    src/support/tii-library \
    src/support/buttons \
    src/support/viterbi-spiral \
    src/output \
    src/scopes \
    src/spectrum-viewer \
    src/file-devices/xml-filewriter \
    src/eti-handler \
    src/service-list \
    src/configuration \
    src/devices \
    src/devices/filereaders/filereader \
    src/devices/filereaders/xml-filereader \
    src/devices/filereaders/raw-files \
    src/devices/filereaders/wav-files \
    src/devices/dummy-handler
	  
INCLUDEPATH += \
    ../dabstar-libs/include\qwt \
    ../dabstar-libs/include + $${DEPENDPATH}

HEADERS += \
    src/main/radio.h \
    src/main/glob_defs.h \
    src/main/glob_enums.h \
    src/main/dab-processor.h \
    src/main/dab-constants.h \
    src/main/mot-content-types.h \
    src/eti-handler/eti-generator.h \
    src/main/data_manip_and_checks.h \
    src/ofdm/sample-reader.h \
    src/ofdm/phasereference.h \
    src/ofdm/ofdm-decoder.h \
    src/ofdm/phasetable.h \
    src/ofdm/freq-interleaver.h \
    src/ofdm/fib-decoder.h \
    src/ofdm/dab-config.h \
    src/ofdm/fib-table.h \
    src/ofdm/fic-handler.h \
    src/ofdm/tii-detector.h \
    src/ofdm/timesyncer.h \
    src/protection/protTables.h \
    src/protection/protection.h \
    src/protection/uep-protection.h \
    src/protection/eep-protection.h \
    src/backend/firecode-checker.h \
    src/backend/frame-processor.h \
    src/backend/charsets.h \
    src/backend/galois.h \
    src/backend/reed-solomon.h \
    src/backend/msc-handler.h \
    src/backend/backend.h \
    src/backend/backend-deconvolver.h \
    src/backend/backend-driver.h \
    src/backend/audio/mp4processor.h \
    src/backend/audio/bitWriter.h \
    src/backend/audio/mp2processor.h \
    src/backend/data/ip-datahandler.h \
    src/backend/data/tdc-datahandler.h \
    src/backend/data/journaline-datahandler.h \
    src/backend/data/journaline/dabdatagroupdecoder.h \
    src/backend/data/journaline/crc_8_16.h \
    src/backend/data/journaline/log.h \
    src/backend/data/journaline/newssvcdec_impl.h \
    src/backend/data/journaline/Splitter.h \
    src/backend/data/journaline/dabdgdec_impl.h \
    src/backend/data/journaline/newsobject.h \
    src/backend/data/journaline/NML.h \
    src/backend/data/epg/epgdec.h \
    src/backend/data/epg-2/epg-decoder.h \
    src/backend/data/virtual-datahandler.h \
    src/backend/data/pad-handler.h \
    src/backend/data/mot/mot-handler.h \
    src/backend/data/mot/mot-object.h \
    src/backend/data/mot/mot-dir.h \
    src/backend/data/data-processor.h \
    src/output/fir-filters.h \
    src/output/audio-player.h \
    src/output/newconverter.h \
    src/output/audiofifo.h \
    src/output/audiooutput.h \
    src/output/audiooutputqt.h \
    src/support/ringbuffer.h \
    src/support/techdata.h \
    src/support/Xtan2.h \
    src/support/dab-params.h \
    src/support/band-handler.h \
    src/support/dab-tables.h \
    src/support/dxDisplay.h \
    src/support/viterbi-spiral/viterbi-spiral.h \
    src/support/color-selector.h \
    src/support/time-table.h \
    src/support/openfiledialog.h \
    src/support/content-table.h \
    src/support/dl-cache.h \
    src/support/ITU_Region_1.h \
    src/support/coordinates.h \
    src/support/mapport.h \
    src/support/http-handler.h \
    src/support/converted_map.h \
    src/support/tii-library/tii-codes.h \
    src/support/buttons/newpushbutton.h \
    src/support/buttons/normalpushbutton.h \
    src/support/buttons/circlepushbutton.h \
    src/support/custom_frame.h \
    src/support/setting-helper.h \
    src/support/converter_48000.h \
    src/support/wav_writer.h \
    src/support/angle_direction.h \
    src/scopes/iqdisplay.h \
    src/scopes/carrier-display.h \
    src/scopes/spectrogramdata.h \
    src/scopes/audio-display.h \
    src/scopes/qwt_defs.h \
    src/spectrum-viewer/spectrum-viewer.h \
    src/spectrum-viewer/spectrum-scope.h \
    src/spectrum-viewer/waterfall-scope.h \
    src/spectrum-viewer/correlation-viewer.h \
    src/file-devices/xml-filewriter/xml-filewriter.h \
    src/service-list/service-list-handler.h \
    src/service-list/service-db.h \
    src/configuration/configuration.h \
    src/output/Qt-audio.h \
    src/output/Qt-audiodevice.h \
    src/devices/device-handler.h \
    src/devices/device-exceptions.h \
    src/devices/device-selector.h \
    src/devices/filereaders/xml-filereader/element-reader.h \
    src/devices/filereaders/xml-filereader/xml-filereader.h \
    src/devices/filereaders/xml-filereader/xml-reader.h \
    src/devices/filereaders/xml-filereader/xml-descriptor.h \
    src/devices/filereaders/raw-files/rawfiles.h \
    src/devices/filereaders/raw-files/raw-reader.h \
    src/devices/filereaders/wav-files/wavfiles.h \
    src/devices/filereaders/wav-files/wav-reader.h \
    src/fft/fft-handler.h \
    src/fft/fft-complex.h \
    src/fft/kiss_fft.h \
    src/fft/kiss_fftr.h

SOURCES += \
    src/main/main.cpp \
    src/main/radio.cpp \
    src/main/dab-processor.cpp \
    src/support/techdata.cpp \
    src/eti-handler/eti-generator.cpp \
    src/ofdm/sample-reader.cpp \
    src/ofdm/ofdm-decoder.cpp \
    src/ofdm/phasereference.cpp \
    src/ofdm/phasetable.cpp \
    src/ofdm/freq-interleaver.cpp \
    src/ofdm/fib-decoder.cpp \
    src/ofdm/fic-handler.cpp \
    src/ofdm/tii-detector.cpp \
    src/ofdm/timesyncer.cpp \
    src/protection/protTables.cpp \
    src/protection/protection.cpp \
    src/protection/eep-protection.cpp \
    src/protection/uep-protection.cpp \
    src/backend/firecode-checker.cpp \
    src/backend/charsets.cpp \
    src/backend/galois.cpp \
    src/backend/reed-solomon.cpp \
    src/backend/msc-handler.cpp \
    src/backend/backend.cpp \
    src/backend/backend-deconvolver.cpp \
    src/backend/backend-driver.cpp \
    src/backend/audio/mp4processor.cpp \
    src/backend/audio/bitWriter.cpp \
    src/backend/audio/mp2processor.cpp \
    src/backend/data/ip-datahandler.cpp \
    src/backend/data/journaline-datahandler.cpp \
    src/backend/data/journaline/crc_8_16.c \
    src/backend/data/journaline/log.c \
    src/backend/data/journaline/newssvcdec_impl.cpp \
    src/backend/data/journaline/Splitter.cpp \
    src/backend/data/journaline/dabdgdec_impl.c \
    src/backend/data/journaline/newsobject.cpp \
    src/backend/data/journaline/NML.cpp \
    src/backend/data/epg-2/epg-decoder.cpp \
    src/backend/data/epg/epgdec.cpp \
    src/backend/data/tdc-datahandler.cpp \
    src/backend/data/pad-handler.cpp \
    src/backend/data/mot/mot-handler.cpp \
    src/backend/data/mot/mot-object.cpp \
    src/backend/data/mot/mot-dir.cpp \
    src/backend/data/data-processor.cpp \
    src/output/audio-player.cpp \
    src/output/newconverter.cpp \
    src/output/fir-filters.cpp \
    src/output/audiofifo.cpp \
    src/output/audiooutput.cpp \
    src/output/audiooutputqt.cpp \
    src/support/ringbuffer.cpp \
    src/support/Xtan2.cpp \
    src/support/dab-params.cpp \
    src/support/band-handler.cpp \
    src/support/dab-tables.cpp \
    src/support/buttons/newpushbutton.cpp \
    src/support/buttons/normalpushbutton.cpp \
    src/support/buttons/circlepushbutton.cpp \
    src/support/viterbi-spiral/viterbi-spiral.cpp \
    src/support/color-selector.cpp \
    src/support/time-table.cpp \
    src/support/openfiledialog.cpp \
    src/support/content-table.cpp \
    src/support/ITU_Region_1.cpp \
    src/support/coordinates.cpp \
    src/support/mapport.cpp \
    src/support/http-handler.cpp \
    src/support/dxDisplay.cpp \
    src/support/tii-library/tii-codes.cpp \
    src/support/custom_frame.cpp \
    src/support/setting-helper.cpp \
    src/support/converter_48000.cpp \
    src/support/wav_writer.cpp \
    src/support/angle_direction.cpp \
    src/scopes/iqdisplay.cpp \
    src/scopes/carrier-display.cpp \
    src/scopes/audio-display.cpp \
    src/scopes/spectrogramdata.cpp \
    src/spectrum-viewer/spectrum-viewer.cpp \
    src/spectrum-viewer/spectrum-scope.cpp \
    src/spectrum-viewer/waterfall-scope.cpp \
    src/spectrum-viewer/correlation-viewer.cpp \
    src/file-devices/xml-filewriter/xml-filewriter.cpp \
    src/service-list/service-list-handler.cpp \
    src/service-list/service-db.cpp \
    src/configuration/configuration.cpp \
    src/output/Qt-audio.cpp \
    src/output/Qt-audiodevice.cpp \
    src/devices/device-selector.cpp \
    src/devices/filereaders/xml-filereader/xml-filereader.cpp \
    src/devices/filereaders/xml-filereader/xml-reader.cpp \
    src/devices/filereaders/xml-filereader/xml-descriptor.cpp \
    src/devices/filereaders/raw-files/rawfiles.cpp \
    src/devices/filereaders/raw-files/raw-reader.cpp \
    src/devices/filereaders/wav-files/wavfiles.cpp \
    src/devices/filereaders/wav-files/wav-reader.cpp \
    src/fft/fft-handler.cpp \
    src/fft/fft-complex.cpp \
    src/fft/kiss_fft.c \
    src/fft/kiss_fftr.c

FORMS += \
    src/main/radio.ui \
    src/spectrum-viewer/spectrum_viewer.ui \
    src/support/technical_data.ui \
    src/configuration/configuration.ui \
    src/devices/filereaders/xml-filereader/xmlfiles.ui
#    src/specials/dumpviewer/dumpwidget.ui 
#    src/specials/sound-client/soundwidget.ui

#DESTDIR		= ../../DABstar-Qt6.7.3
LIBS		+= -L../../dabstar-libs/lib
CONFIG		+= airspy
#CONFIG		+= spyServer-16
#CONFIG		+= spyServer-8
CONFIG		+= rtl_tcp
CONFIG		+= dabstick
#CONFIG		+= sdrplay-v2
#CONFIG		+= pluto
CONFIG		+= sdrplay-v3
#CONFIG		+= hackrf
#CONFIG		+= lime
CONFIG		+= VITERBI_SSE
CONFIG		+= faad
#CONFIG		+= fdk-aac
LIBS		+= -lsndfile-1
LIBS		+= -lsamplerate
LIBS		+= -lwinpthread
LIBS		+= -lws2_32
#LIBS		+= -lusb-1.0
LIBS		+= -lzlib
LIBS		+= -ldl
LIBS		+= -lfftw3f-3
LIBS		+= -lqwt

# very experimental, simple server for connecting to a tdc handler
#CONFIG		+= datastreamer

#	dabstick
#	Note: the windows version is bound to the dll, the
#	linux version loads the function from the so
dabstick {
	DEFINES		+= HAVE_RTLSDR
	DEPENDPATH	+= src/devices/rtlsdr-handler
	INCLUDEPATH	+= src/devices/rtlsdr-handler
	HEADERS		+= src/devices/rtlsdr-handler/rtlsdr-handler.h \
	                   src/devices/rtlsdr-handler/rtl-dongleselect.h
	SOURCES		+= src/devices/rtlsdr-handler/rtlsdr-handler.cpp \
	                   src/devices/rtlsdr-handler/rtl-dongleselect.cpp 
	FORMS		+= src/devices/rtlsdr-handler/rtlsdr-widget.ui
}

#
#	the SDRplay
#
sdrplay-v2 {
	DEFINES		+= HAVE_SDRPLAY_V2
	DEPENDPATH	+= src/devices/sdrplay-handler-v2
	INCLUDEPATH	+= src/devices/sdrplay-handler-v2
	HEADERS		+= src/devices/sdrplay-handler-v2/sdrplay-handler-v2.h \
	                   src/devices/sdrplay-handler-v2/sdrplayselect.h 
	SOURCES		+= src/devices/sdrplay-handler-v2/sdrplay-handler-v2.cpp \
	                   src/devices/sdrplay-handler-v2/sdrplayselect.cpp 
	FORMS		+= src/devices/sdrplay-handler-v2/sdrplay-widget-v2.ui
}

#
#	the SDRplay
#
sdrplay-v3 {
	DEFINES		+= HAVE_SDRPLAY_V3
	DEPENDPATH	+= src/devices/sdrplay-handler-v3
	INCLUDEPATH	+= src/devices/sdrplay-handler-v3 \
			   ../dabstar-libs/include\sdrplay
        HEADERS         += src/devices/sdrplay-handler-v3/sdrplay-handler-v3.h \
                           src/devices/sdrplay-handler-v3/sdrplay-commands.h \
	                   src/devices/sdrplay-handler-v3/Rsp-device.h \
	                   src/devices/sdrplay-handler-v3/Rsp1-handler.h \
	                   src/devices/sdrplay-handler-v3/Rsp1A-handler.h \
	                   src/devices/sdrplay-handler-v3/Rsp2-handler.h \
	                   src/devices/sdrplay-handler-v3/RspDuo-handler.h \
	                   src/devices/sdrplay-handler-v3/RspDx-handler.h
        SOURCES         += src/devices/sdrplay-handler-v3/Rsp-device.cpp \
	                   src/devices/sdrplay-handler-v3/sdrplay-handler-v3.cpp \
	                   src/devices/sdrplay-handler-v3/Rsp1-handler.cpp \
	                   src/devices/sdrplay-handler-v3/Rsp1A-handler.cpp \
	                   src/devices/sdrplay-handler-v3/Rsp2-handler.cpp \
	                   src/devices/sdrplay-handler-v3/RspDuo-handler.cpp \
	                   src/devices/sdrplay-handler-v3/RspDx-handler.cpp 
	FORMS		+= src/devices/sdrplay-handler-v3/sdrplay-widget-v3.ui
}

#
#	limeSDR
#
lime  {
	DEFINES		+= HAVE_LIME
	INCLUDEPATH	+= src/devices/lime-handler
	DEPENDPATH	+= src/devices/lime-handler
        HEADERS         += src/devices/lime-handler/lime-handler.h \	
	                   src/devices/lime-handler/lime-widget.h
        SOURCES         += src/devices/lime-handler/lime-handler.cpp 
}

#
#	the hackrf
#
hackrf {
	DEFINES		+= HAVE_HACKRF
	DEPENDPATH	+= src/devices/hackrf-handler 
	INCLUDEPATH	+= src/devices/hackrf-handler 
	HEADERS		+= src/devices/hackrf-handler/hackrf-handler.h 
	SOURCES		+= src/devices/hackrf-handler/hackrf-handler.cpp 
	FORMS		+= src/devices/hackrf-handler/hackrf-widget.ui
}

#
# airspy support
#
airspy {
	DEFINES		+= HAVE_AIRSPY
	DEPENDPATH	+= src/devices/airspy 
	INCLUDEPATH	+= src/devices/airspy-handler \
	                   src/devices/airspy-handler/libairspy
	HEADERS		+= src/devices/airspy-handler/airspy-handler.h \
	                   src/devices/airspy-handler/airspyselect.h \
	                   src/devices/airspy-handler/libairspy/airspy.h
	SOURCES		+= src/devices/airspy-handler/airspy-handler.cpp \
	                   src/devices/airspy-handler/airspyselect.cpp
	FORMS		+= src/devices/airspy-handler/airspy-widget.ui
}

airspy-2 {
	DEFINES		+= HAVE_AIRSPY_2
	DEPENDPATH	+= src/devices/airspy-2 
	INCLUDEPATH	+= src/devices/airspy-2 \
	                   src/devices/airspy-2/libairspy
	HEADERS		+= src/devices/airspy-2/airspy-2.h \
	                   src/devices/airspy-2/airspyselect.h \
	                   src/devices/airspy-2/libairspy/airspy.h
	SOURCES		+= src/devices/airspy-2/airspy-2.cpp \
	                   src/devices/airspy-2/airspyselect.cpp
	FORMS		+= src/devices/airspy-2/airspy-widget.ui
}

#	extio dependencies, windows only
#
extio {
	DEFINES		+= HAVE_EXTIO
	INCLUDEPATH	+= src/devices/extio-handler
	HEADERS		+= src/devices/extio-handler/extio-handler.h \
	                   src/devices/extio-handler/common-readers.h \
	                   src/devices/extio-handler/virtual-reader.h
	SOURCES		+= src/devices/extio-handler/extio-handler.cpp \
	                   src/devices/extio-handler/common-readers.cpp \
	                   src/devices/extio-handler/virtual-reader.cpp
}

#
rtl_tcp {
	DEFINES		+= HAVE_RTL_TCP
	QT		+= network
	INCLUDEPATH	+= src/devices/rtl_tcp
	HEADERS		+= src/devices/rtl_tcp/rtl_tcp_client.h
	SOURCES		+= src/devices/rtl_tcp/rtl_tcp_client.cpp
	FORMS		+= src/devices/rtl_tcp/rtl_tcp-widget.ui
}

soapy {
	DEFINES		+= HAVE_SOAPY
	DEPENDPATH	+= src/devices/soapy
	INCLUDEPATH     += src/devices/soapy
        HEADERS         += src/devices/soapy/soapy-handler.h \
	                   src/devices/soapy/soapy-converter.h
        SOURCES         += src/devices/soapy/soapy-handler.cpp \
	                   src/devices/soapy/soapy-converter.cpp
        FORMS           += src/devices/soapy/soapy-widget.ui
	LIBS		+= -lSoapySDR -lm
}

pluto-rxtx	{
	DEFINES		+= HAVE_PLUTO_RXTX
	QT		+= network
	INCLUDEPATH	+= src/devices/pluto-rxtx
	INCLUDEPATH	+= src/devices/pluto-rxtx/dab-streamer
	HEADERS		+= src/devices/pluto-rxtx/dabFilter.h
	HEADERS		+= src/devices/pluto-rxtx/pluto-rxtx-handler.h 
	HEADERS		+= src/devices/pluto-rxtx/dab-streamer/dab-streamer.h 
	HEADERS		+= src/devices/pluto-rxtx/dab-streamer/up-filter.h
	SOURCES		+= src/devices/pluto-rxtx/pluto-rxtx-handler.cpp 
	SOURCES		+= src/devices/pluto-rxtx/dab-streamer/dab-streamer.cpp
	SOURCES		+= src/devices/pluto-rxtx/dab-streamer/up-filter.cpp
	FORMS		+= src/devices/pluto-rxtx/pluto-rxtx-widget.ui
	LIBS		+= -liio -lad9361
}

pluto	{
	DEFINES		+= HAVE_PLUTO
	QT		+= network
	INCLUDEPATH	+= src/devices/pluto-handler
	HEADERS		+= src/devices/pluto-handler/dabFilter.h
	HEADERS		+= src/devices/pluto-handler/pluto-handler.h
	SOURCES		+= src/devices/pluto-handler/pluto-handler.cpp
	FORMS		+= src/devices/pluto-handler/pluto-widget.ui
}

elad-device	{
	DEFINES		+= HAVE_ELAD
	DEPENDPATH	+= src/devices/elad-s1-handler
	INCLUDEPATH	+= src/devices/elad-s1-handler
	HEADERS		+= src/devices/elad-s1-handler/elad-handler.h
	HEADERS		+= src/devices/elad-s1-handler/elad-loader.h
	HEADERS		+= src/devices/elad-s1-handler/elad-worker.h
	SOURCES		+= src/devices/elad-s1-handler/elad-handler.cpp
	SOURCES		+= src/devices/elad-s1-handler/elad-loader.cpp
	SOURCES		+= src/devices/elad-s1-handler/elad-worker.cpp
	FORMS		+= src/devices/elad-s1-handler/elad-widget.ui
}

spyServer-8  {
	DEFINES		+= HAVE_SPYSERVER_8
	DEPENDPATH	+= src/devices/spy-server-8
	INCLUDEPATH	+= src/devices/spy-server-8
	HEADERS		+= src/devices/spy-server-8/spyserver-protocol.h 
	HEADERS		+= src/devices/spy-server-8/tcp-client-8.h 
	HEADERS		+= src/devices/spy-server-8/spy-handler-8.h 
	HEADERS		+= src/devices/spy-server-8/spyserver-client-8.h
	SOURCES		+= src/devices/spy-server-8/tcp-client-8.cpp 
	SOURCES		+= src/devices/spy-server-8/spy-handler-8.cpp 
	SOURCES		+= src/devices/spy-server-8/spyserver-client-8.cpp 
	FORMS		+= src/devices/spy-server-8/spyserver-widget-8.ui
}
	
spyServer-16  {
	DEFINES		+= HAVE_SPYSERVER_16
	DEPENDPATH	+= src/devices/spy-server-16
	INCLUDEPATH	+= src/devices/spy-server-16
	HEADERS		+= src/devices/spy-server-16/spyserver-protocol.h 
	HEADERS		+= src/devices/spy-server-16/tcp-client.h 
	HEADERS		+= src/devices/spy-server-16/spy-handler.h 
	HEADERS		+= src/devices/spy-server-16/spyserver-client.h
	SOURCES		+= src/devices/spy-server-16/tcp-client.cpp 
	SOURCES		+= src/devices/spy-server-16/spy-handler.cpp 
	SOURCES		+= src/devices/spy-server-16/spyserver-client.cpp 
	FORMS		+= src/devices/spy-server-16/spyserver-widget.ui
}
	
uhd	{
	DEFINES		+= HAVE_UHD
	DEPENDPATH	+= src/devices/uhd
	INCLUDEPATH	+= src/devices/uhd
	HEADERS		+= src/devices/uhd/uhd-handler.h
	SOURCES		+= src/devices/uhd/uhd-handler.cpp
	FORMS		+= src/devices/uhd/uhd-widget.ui
	LIBS		+= -luhd
}

colibri	{
	DEFINES		+= HAVE_COLIBRI
	DEPENDPATH	+= src/devices/colibri-handler
	INCLUDEPATH	+= src/devices/colibri-handler
	HEADERS		+= src/devices/colibri-handler/common.h
	HEADERS		+= src/devices/colibri-handler/LibLoader.h
	HEADERS		+= src/devices/colibri-handler/colibri-handler.h
	SOURCES		+= src/devices/colibri-handler/LibLoader.cpp
	SOURCES		+= src/devices/colibri-handler/colibri-handler.cpp
	FORMS		+= src/devices/colibri-handler/colibri-widget.ui
}
	
datastreamer	{
	DEFINES		+= DATA_STREAMER
	DEFINES		+= CLOCK_STREAMER
	INCLUDEPATH	+= src/server-thread
	HEADERS		+= src/server-thread/tcp-server.h
	SOURCES		+= src/server-thread/tcp-server.cpp
}

VITERBI_SSE	{
	DEFINES		+= SSE_AVAILABLE
	HEADERS		+= src/support/viterbi-spiral/spiral-sse.h
	SOURCES		+= src/support/viterbi-spiral/spiral-sse.c
}

NO_SSE	{
	HEADERS		+= src/support/viterbi-spiral/spiral-no-sse.h
	SOURCES		+= src/support/viterbi-spiral/spiral-no-sse.c
}

faad	{
	HEADERS		+= src/backend/audio/faad-decoder.h
	SOURCES		+= src/backend/audio/faad-decoder.cpp
	LIBS		+= -lfaad-2
}

fdk-aac {
        DEFINES         += __WITH_FDK_AAC__
        INCLUDEPATH     += ../dabstar-libs/include\fdk-aac
#        INCLUDEPATH     += src/specials/fdk-aac
        HEADERS         += src/backend/audio/fdk-aac.h
        SOURCES         += src/backend/audio/fdk-aac.cpp
	LIBS		+= -lfdk-aac.dll
}
