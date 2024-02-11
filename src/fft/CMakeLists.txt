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
