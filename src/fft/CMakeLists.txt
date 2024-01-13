message(STATUS "Begin: It's me the FFT! ----------------------")

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


#list(APPEND extraLibs ${FFTW3F_LIBRARIES})

target_link_libraries(${objectName}_fft ${FFTW3F_LIBRARIES})

message(STATUS "End:   It's me the FFT! ${objectName}_fft ----------------------")
