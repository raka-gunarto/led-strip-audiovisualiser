cmake_minimum_required(VERSION 3.1)

project(
    AudioVisualiser
    VERSION 1.0
    LANGUAGES C
)

find_library(FFTW3 NAMES fftw3 REQUIRED)
find_library(PORTAUDIO NAMES portaudio REQUIRED)

add_executable(audiovisualiser main.c cava/cavacore.c)
target_link_libraries(audiovisualiser fftw3 portaudio m)

