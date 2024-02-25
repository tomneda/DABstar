
if (USE_FFTW3)
    find_package(FFTW3f)
    if (NOT FFTW3F_FOUND)
        message(FATAL_ERROR "please install FFTW3")
    endif ()
    include_directories(${FFTW3F_INCLUDE_DIRS})
    list(APPEND extraLibs ${FFTW3F_LIBRARIES})

    add_definitions(-D__FFTW3__)
else ()
    add_definitions(-D__KISS_FFT__)
endif ()

add_library(${objectName}_fft STATIC
        ./fft-handler.h
        ./fft-complex.h
        ./fft-handler.cpp
        ./fft-complex.cpp
        ./kiss_fft.h
        ./kiss_fftr.h
        ./kiss_fft.c
        ./kiss_fftr.c
)

target_link_libraries(${objectName}_fft PRIVATE ${FFTW3F_LIBRARIES})

target_include_directories(${objectName}_fft INTERFACE .)
