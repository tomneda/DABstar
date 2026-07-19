
set(baseLibName ${objectName}_base)

find_package(FFTW3F)
if (NOT FFTW3F_FOUND)
    message(FATAL_ERROR "Please install FFTW3")
endif ()

search_for_library(ZLIB zlib)
search_for_library(LIBSNDFILE sndfile)

set(base_ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/cir_widget.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/configuration.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/dabradio.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/dumpwidget.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/spectrum_viewer.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/techdata.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/updatedialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/ensemble_list.ui
)


if (SSE_OR_AVX)
    set(${baseLibName}_HDRS
            ${${baseLibName}_HDRS}
            support/simd_extensions.h
            ofdm/ofdm_decoder_simd.h
    )
    set(${baseLibName}_SRCS
            ${${baseLibName}_SRCS}
            ofdm/ofdm_decoder_simd.cpp
    )
    find_package(Volk REQUIRED)
    list(APPEND baseExtraLibs ${VOLK_LIBRARIES})
else ()
    set(${baseLibName}_HDRS
            ${${baseLibName}_HDRS}
            ofdm/ofdm_decoder.h
    )
    set(${baseLibName}_SRCS
            ${${baseLibName}_SRCS}
            ofdm/ofdm_decoder.cpp
    )
endif ()

if (VITERBI_AVX2)
    set(${baseLibName}_HDRS
            ${${baseLibName}_HDRS}
            support/viterbi_spiral/viterbi_16way.h
    )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
elseif (VITERBI_SSE2)
    set(${baseLibName}_HDRS
            ${${baseLibName}_HDRS}
            support/viterbi_spiral/viterbi_8way.h
    )
elseif (VITERBI_NEON)
    set(${baseLibName}_HDRS
            ${${baseLibName}_HDRS}
            support/viterbi_spiral/sse2neon.h
    )
else ()
    set(${baseLibName}_HDRS
            ${${baseLibName}_HDRS}
            support/viterbi_spiral/viterbi_scalar.h
    )
endif ()

if (DATA_STREAMER)
    set(${baseLibName}_HDRS
            ${${baseLibName}_HDRS}
            server_thread/tcp_server.h
    )
    set(${baseLibName}_SRCS
            ${${baseLibName}_SRCS}
            server_thread/tcp_server.cpp
    )
endif ()

if (FDK_AAC)
    find_package(Fdk-AAC)
    if (NOT LIBFDK_AAC_FOUND)
        message(FATAL_ERROR "Please install libfdk-aac")
    endif ()
    set(${baseLibName}_HDRS
            ${${baseLibName}_HDRS}
            backend/audio/fdk_aac.h
    )
    set(${baseLibName}_SRCS
            ${${baseLibName}_SRCS}
            backend/audio/fdk_aac.cpp
    )
    list(APPEND baseExtraLibs ${FDK_AAC_LIBRARIES})
else ()
    find_package(Faad)
    if (NOT FAAD_FOUND)
        message(FATAL_ERROR "Please install libfaad")
    endif ()
    set(${baseLibName}_HDRS
            ${${baseLibName}_HDRS}
            backend/audio/faad_decoder.h
    )
    set(${baseLibName}_SRCS
            ${${baseLibName}_SRCS}
            backend/audio/faad_decoder.cpp
    )
    list(APPEND baseExtraLibs ${FAAD_LIBRARIES})
endif ()

#########################################################################

set(${baseLibName}_HDRS
        ${${baseLibName}_HDRS}
        main/dabradio.h
        main/audio_manager.h
        main/epg_mot_handler.h
        main/tii_manager.h
        main/gap_progress_bar.h
        main/dab_channel_desc.h
        main/glob_enums.h
        main/dab_processor.h
        main/mot_content_types.h
        main/mot_slide_progress.h
        eti_handler/eti_generator.h
        main/bit_extractors.h
        ofdm/sample_reader.h
        ofdm/phasereference.h
        ofdm/phasetable.h
        ofdm/freq_interleaver.h
        decoder/fib_decoder_if.h
        decoder/fib_decoder.h
        decoder/fib_config_fig0.h
        decoder/fib_config_fig1.h
        decoder/fib_table.h
        decoder/fic_decoder.h
        decoder/fib_helper.h
        ofdm/tii_detector.h
        ofdm/timesyncer.h
        protection/protTables.h
        protection/protection.h
        protection/uep_protection.h
        protection/eep_protection.h
        backend/firecode_checker.h
        backend/crc.h
        backend/frame_processor.h
        backend/charsets.h
        backend/galois.h
        backend/reed_solomon.h
        backend/msc_handler.h
        backend/backend.h
        backend/backend_deconvolver.h
        backend/backend_driver.h
        backend/audio/mp4processor.h
        backend/audio/bit_writer.h
        backend/audio/mp2processor.h
        backend/data/ip_datahandler.h
        backend/data/tdc_datahandler.h
        backend/data/journaline_datahandler.h
        backend/data/journaline_viewer.h
        backend/data/journaline/dabdatagroupdecoder.h
        backend/data/journaline/crc_8_16.h
        backend/data/journaline/log.h
        backend/data/journaline/newssvcdec_impl.h
        backend/data/journaline/Splitter.h
        backend/data/journaline/dabdgdec_impl.h
        backend/data/journaline/newsobject.h
        backend/data/journaline/NML.h
        backend/data/epg/epgdec.h
        backend/data/epg_2/epg_decoder.h
        backend/data/virtual_datahandler.h
        backend/data/pad_handler.h
        backend/data/mot/mot_handler.h
        backend/data/mot/mot_object.h
        backend/data/mot/mot_dir.h
        backend/data/data_processor.h
        audio/audiofifo.h
        audio/audiooutput_if.h
        audio/audioiodevice.h
        audio/test_tone.h
        audio/audiooutputqt.h
        support/band_handler.h
        support/dab_tables.h
        support/tii_list_display.h
        support/viterbi_spiral/viterbi_spiral.h
        support/color_selector.h
        support/time_table.h
        support/content_table.h
        support/dl_cache.h
        support/itu_regions.h
        support/map_http_server.h
        support/tii_library/tii_codes.h
        support/custom_frame.h
        support/gui_helpers.h
        support/wav_writer.h
        support/compass_direction.h
        support/time_meas.h
        support/copyright_info.h
        scopes/iqdisplay.h
        scopes/carrier_display.h
        scopes/audio_display.h
        scopes/plot_widget.h
        scopes/level_meter.h
        spectrum_viewer/spectrum_viewer.h
        spectrum_viewer/cir_viewer.h
        spectrum_viewer/spectrum_scope.h
        spectrum_viewer/waterfall_scope.h
        spectrum_viewer/correlation_viewer.h
        service_list/service_list_handler.h
        service_list/service_db.h
        configuration/configuration.h
        ensemble_list/ensemble_list.h
        ensemble_list/ensemble_list_db.h
        ensemble_list/ensemble_list_db_handler.h
        update/updatechecker.h
        update/updatedialog.h
)

set(${baseLibName}_SRCS
        ${${baseLibName}_SRCS}
        # main/main.cpp intentionally NOT here: the entry point is compiled
        # directly into the executable target (see top-level CMakeLists.txt),
        # otherwise LTO cannot extract main() from this static archive.
        main/dabradio.cpp
        main/dabradio_ui.cpp
        main/dabradio_dump.cpp
        main/audio_manager.cpp
        main/epg_mot_handler.cpp
        main/tii_manager.cpp
        main/dabradio_el.cpp
        main/dabradio_ctrl.cpp
        main/dab_channel_desc.cpp
        main/dab_processor.cpp
        support/techdata.cpp
        main/mot_slide_progress.cpp
        eti_handler/eti_generator.cpp
        ofdm/sample_reader.cpp
        ofdm/phasereference.cpp
        ofdm/phasetable.cpp
        ofdm/freq_interleaver.cpp
        decoder/fib_config_fig0.cpp
        decoder/fib_config_fig1.cpp
        decoder/fib_decoder.cpp
        decoder/fib_decoder_fig0.cpp
        decoder/fib_decoder_fig1.cpp
        decoder/fib_decoder_string_getter.cpp
        decoder/fic_decoder.cpp
        decoder/fib_helper.cpp
        ofdm/tii_detector.cpp
        ofdm/timesyncer.cpp
        protection/protTables.cpp
        protection/protection.cpp
        protection/eep_protection.cpp
        protection/uep_protection.cpp
        backend/firecode_checker.cpp
        backend/crc.cpp
        backend/charsets.cpp
        backend/galois.cpp
        backend/reed_solomon.cpp
        backend/msc_handler.cpp
        backend/backend.cpp
        backend/backend_deconvolver.cpp
        backend/backend_driver.cpp
        backend/audio/mp4processor.cpp
        backend/audio/bit_writer.cpp
        backend/audio/mp2processor.cpp
        backend/data/ip_datahandler.cpp
        backend/data/journaline_datahandler.cpp
        backend/data/journaline_viewer.cpp
        backend/data/journaline/crc_8_16.c
        backend/data/journaline/log.c
        backend/data/journaline/newssvcdec_impl.cpp
        backend/data/journaline/Splitter.cpp
        backend/data/journaline/dabdgdec_impl.c
        backend/data/journaline/newsobject.cpp
        backend/data/journaline/NML.cpp
        backend/data/epg_2/epg_decoder.cpp
        backend/data/epg/epgdec.cpp
        backend/data/tdc_datahandler.cpp
        backend/data/pad_handler.cpp
        backend/data/mot/mot_handler.cpp
        backend/data/mot/mot_object.cpp
        backend/data/mot/mot_dir.cpp
        backend/data/data_processor.cpp
        audio/audioiodevice.cpp
        audio/test_tone.cpp
        audio/audiooutputqt.cpp
        support/ringbuffer.cpp
        support/band_handler.cpp
        support/dab_tables.cpp
        support/viterbi_spiral/viterbi_spiral.cpp
        support/color_selector.cpp
        support/time_table.cpp
        support/content_table.cpp
        support/dl_cache.cpp
        support/itu_regions.cpp
        support/map_http_server.cpp
        support/tii_list_display.cpp
        support/tii_library/tii_codes.cpp
        support/custom_frame.cpp
        support/gui_helpers.cpp
        support/wav_writer.cpp
        support/compass_direction.cpp
        support/copyright_info.cpp
        scopes/iqdisplay.cpp
        scopes/carrier_display.cpp
        scopes/audio_display.cpp
        scopes/plot_widget.cpp
        scopes/level_meter.cpp
        spectrum_viewer/spectrum_viewer.cpp
        spectrum_viewer/spectrum_scope.cpp
        spectrum_viewer/waterfall_scope.cpp
        spectrum_viewer/correlation_viewer.cpp
        spectrum_viewer/cir_viewer.cpp
        service_list/service_list_handler.cpp
        service_list/service_db.cpp
        configuration/configuration.cpp
        ensemble_list/ensemble_list.cpp
        ensemble_list/ensemble_list_db.cpp
        ensemble_list/ensemble_list_db_handler.cpp
        update/updatechecker.cpp
        update/updatedialog.cpp
)

qt6_wrap_ui(${baseLibName}_UI_HDRS
        ${base_ui}
)

set(${baseLibName}_HDRS
        ${${baseLibName}_HDRS}
        ${${baseLibName}_UI_HDRS}
)

add_library(${baseLibName} STATIC ${${baseLibName}_SRCS} ${${baseLibName}_HDRS})

if (extraLibDirs)
  target_link_directories(${baseLibName} PUBLIC ${extraLibDirs})
endif ()

target_include_directories(${baseLibName}
        PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        main
        ofdm
        decoder
        protection
        backend
        backend/audio
        backend/data
        backend/data/journaline
        backend/data/mot
        backend/data/epg_2
        backend/data/epg
        support
        support/tii_library
        support/buttons
        support/viterbi_spiral
        audio
        scopes
        spectrum_viewer
        eti_handler
        service_list
        configuration
        ensemble_list
        update
        server_thread
        PRIVATE
        ${FFTW3F_INCLUDE_DIRS}
        ${FDK_AAC_INCLUDE_DIR}/fdk-aac
        ${FAAD_INCLUDE_DIRS}
        ${VOLK_INCLUDE_DIRS}
)

target_link_libraries(${baseLibName}
        PUBLIC
        ${objectName}_common
        Qt6::Widgets
        Qt6::Xml
        Qt6::Sql
        Qt6::Multimedia
        Qt6::Charts
        Qt6::Network
        ${FFTW3F_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ${LIBSNDFILE_LIBRARIES}
        ${baseExtraLibs}
        ${CMAKE_DL_LIBS}
)
