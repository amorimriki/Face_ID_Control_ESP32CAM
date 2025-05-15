cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Face_ID_Control_ESP32CAM)

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_SOURCE_DIR}/components  # Diretório onde os seus componentes adicionais (ESP-DL e ESP-WHO) estão
)

# Instrução para adicionar o ESP-DL e ESP-WHO como componentes externos
list(APPEND EXTRA_COMPONENT_DIRS 
    ${CMAKE_SOURCE_DIR}/lib/esp-dl
    ${CMAKE_SOURCE_DIR}/lib/esp-who
)