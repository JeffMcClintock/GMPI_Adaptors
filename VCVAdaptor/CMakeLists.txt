cmake_minimum_required(VERSION 3.30)

# VCVAdaptor — header-only common code for running VCV Rack modules inside
# GMPI plugins. Unlike VST3Adaptor (a standalone plugin that hosts VST3s at
# runtime), this is a compile-time library: a plugin repo compiles upstream
# Rack module sources against rack/plugin.hpp and registers them through
# VcvAdaptor.h.
#
# Consumers link the INTERFACE target:
#     target_link_libraries(MyModule PRIVATE VCVAdaptor)
# The GMPI Core headers (Processor.h) are expected on the consumer's include
# path already, which gmpi_plugin() guarantees.

project(VCVAdaptor)

add_library(VCVAdaptor INTERFACE)

# Both the adaptor header and the mock are exposed. rack/ is on the path so
# upstream sources' `#include "plugin.hpp"` resolves to the mock unchanged.
target_include_directories(VCVAdaptor INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/rack
)

# Shown in IDEs even though nothing compiles here.
target_sources(VCVAdaptor INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/VcvAdaptor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rack/plugin.hpp
)
