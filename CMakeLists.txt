cmake_minimum_required(VERSION 3.22)
project(hfc)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(GEB_HFC src/GEB_HFC.cpp src/global.h src/HFC.cpp src/HFC.h)
set_property(TARGET GEB_HFC PROPERTY CXX_STANDARD 20)
target_link_libraries(GEB_HFC z bz2)
install(TARGETS GEB_HFC
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)
