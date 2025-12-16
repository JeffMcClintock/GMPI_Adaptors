cmake_minimum_required(VERSION 3.30)
set(GMPI_SDK_FOLDER_OVERRIDE "" CACHE PATH "path of a local GMPI repo, or blank to fetch it automatically.")
set(GMPI_UI_FOLDER_OVERRIDE "" CACHE PATH "path of a local GMPI UI repo, or blank to fetch it automatically.")
set(SE_LOCAL_BUILD FALSE CACHE BOOL "Execute extra build steps for developers machine")

# CPM bootstrap (shared cache across builds)
set(CPM_DOWNLOAD_VERSION 0.38.6)
set(CPM_USE_NAMED_CACHE_DIRECTORIES ON)
set(CPM_GIT_SHALLOW ON) # shallow clone for speed

if(NOT DEFINED CPM_SOURCE_CACHE)
  if(WIN32)
    set(CPM_SOURCE_CACHE "$ENV{USERPROFILE}/.cpm" CACHE STRING "CPM cache dir")
  else()
    set(CPM_SOURCE_CACHE "$ENV{HOME}/.cache/CPM" CACHE STRING "CPM cache dir")
  endif()
endif()

set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
  file(DOWNLOAD
    "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
    "${CPM_DOWNLOAD_LOCATION}"
    TLS_VERIFY ON
  )
endif()
include("${CPM_DOWNLOAD_LOCATION}")

# Fetch VST3 SDK via CPM
CPMAddPackage(
NAME vst3_sdk
GITHUB_REPOSITORY steinbergmedia/vst3sdk
GIT_TAG v3.7.14_build_55       # or a specific commit SHA
DOWNLOAD_ONLY YES              # we only need sources; actual add_subdirectory happens later
)
set(VST3_SDK ${vst3_sdk_SOURCE_DIR})

# GMPI SDK
if("${GMPI_SDK_FOLDER_OVERRIDE}" STREQUAL "")
message(STATUS "Fetching GMPI from github")
#note: SOURCE_SUBDIR is a subfolder with NO cmake file (so we don't needlessly generate projects for the GMPI examples)
FetchContent_Declare(
  gmpi
  GIT_REPOSITORY https://github.com/JeffMcClintock/GMPI
  GIT_TAG origin/main
  SOURCE_SUBDIR Core
)
FetchContent_MakeAvailable(gmpi)
set(GMPI_SDK ${gmpi_SOURCE_DIR})
else()
message(STATUS "Using local GMPI folder")
set(GMPI_SDK ${GMPI_SDK_FOLDER_OVERRIDE})
endif()

# GMPI-UI SDK
if("${GMPI_UI_FOLDER_OVERRIDE}" STREQUAL "")
message(STATUS "Fetching GMPI-UI from github")
#note: SOURCE_SUBDIR is a subfolder with NO cmake file (so we don't needlessly generate projects for the GMPI examples)
FetchContent_Declare(
  gmpi_ui
  GIT_REPOSITORY https://github.com/JeffMcClintock/gmpi_ui
  GIT_TAG origin/main
  SOURCE_SUBDIR helpers
)
FetchContent_MakeAvailable(gmpi_ui)
set(GMPI_UI_SDK ${gmpi_ui_SOURCE_DIR})
else()
message(STATUS "Using local GMPI-UI folder")
set(GMPI_UI_SDK ${GMPI_UI_FOLDER_OVERRIDE})
endif()

project(VST3_Wrapper)

add_definitions(-D_UNICODE)

if (MSVC)
    # Floating Point Model: Fast (/fp:fast)
    # Buffer Security Check: No (/GS-)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /GS-")
endif()

set(vst3_sdk_srcs
${VST3_SDK}/public.sdk/source/vst/hosting/hostclasses.h
${VST3_SDK}/public.sdk/source/vst/hosting/hostclasses.cpp
${VST3_SDK}/public.sdk/source/vst/hosting/module.h
${VST3_SDK}/public.sdk/source/vst/hosting/module.cpp
${VST3_SDK}/public.sdk/source/vst/hosting/processdata.h
${VST3_SDK}/public.sdk/source/vst/hosting/processdata.cpp
${VST3_SDK}/public.sdk/source/vst/hosting/pluginterfacesupport.h
${VST3_SDK}/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
${VST3_SDK}/public.sdk/source/vst/utility/stringconvert.h
${VST3_SDK}/public.sdk/source/vst/utility/stringconvert.cpp
${VST3_SDK}/public.sdk/source/common/commonstringconvert.h
${VST3_SDK}/public.sdk/source/common/commonstringconvert.cpp
${VST3_SDK}/public.sdk/source/common/commoniids.cpp
${VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp
)

IF(WIN32)
set(vst3_sdk_srcs ${vst3_sdk_srcs}
${VST3_SDK}/public.sdk/source/vst/hosting/module_win32.cpp
${VST3_SDK}/public.sdk/samples/vst-hosting/editorhost/source/platform/win32/platform.cpp
${VST3_SDK}/public.sdk/samples/vst-hosting/editorhost/source/platform/win32/window.h
${VST3_SDK}/public.sdk/samples/vst-hosting/editorhost/source/platform/win32/window.cpp
)
ENDIF()

set(srcs
${vst3_sdk_srcs}
${GMPI_SDK}/Core/Common.h
${GMPI_SDK}/Core/RefCountMacros.h
${GMPI_SDK}/Core/GmpiApiCommon.h
${GMPI_SDK}/Core/GmpiSdkCommon.h
${GMPI_SDK}/Core/Processor.h
${GMPI_SDK}/Core/Processor.cpp
${GMPI_SDK}/Core/GmpiApiAudio.h
${GMPI_UI_SDK}/GmpiApiDrawing.h
${GMPI_SDK}/Core/GmpiApiEditor.h
${GMPI_UI_SDK}/Drawing.h

VstFactory.h
VstFactory.cpp
ControllerWrapper.h
ControllerWrapper.cpp
ProcessorWrapper.h
ProcessorWrapper.cpp
WindowManager.h
WindowManager.cpp
AaVstWrapperDiagGui.h
EditButtonGui.h
VstwrapperfailGui.h

${GMPI_SDK}/Hosting/message_queues.cpp

${GMPI_UI_SDK}/helpers/Timer.h
${GMPI_UI_SDK}/helpers/Timer.cpp
)

include_directories(
${VST3_SDK}
${GMPI_SDK}
${GMPI_SDK}/Core
${GMPI_UI_SDK}
${GMPI_UI_SDK}/helpers
)

source_group(sdk_se FILES ${sdk_srcs})
source_group(sdk_vst3 FILES ${vst3_sdk_srcs})

if(APPLE)
FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation )
MARK_AS_ADVANCED (COREFOUNDATION_LIBRARY)
FIND_LIBRARY(QUARTZCORE_LIBRARY QuartzCore )
MARK_AS_ADVANCED (QUARTZCORE_LIBRARY)
FIND_LIBRARY(COCOA_LIBRARY Cocoa )
MARK_AS_ADVANCED (COCOA_LIBRARY)
FIND_LIBRARY(CORETEXT_LIBRARY CoreText )
MARK_AS_ADVANCED (CORETEXT_LIBRARY)
FIND_LIBRARY(OPENGL_LIBRARY OpenGL )
MARK_AS_ADVANCED (OPENGL_LIBRARY)
FIND_LIBRARY(IMAGEIO_LIBRARY ImageIO )
MARK_AS_ADVANCED (IMAGEIO_LIBRARY)
FIND_LIBRARY(COREGRAPHICS_LIBRARY CoreGraphics )
MARK_AS_ADVANCED (COREGRAPHICS_LIBRARY)
FIND_LIBRARY(ACCELERATE_LIBRARY Accelerate )
MARK_AS_ADVANCED (ACCELERATE_LIBRARY)
endif()

#include (GenerateExportHeader)

add_library(${PROJECT_NAME} MODULE ${sdk_srcs} ${srcs} )

# 'DEBUG' is a steinberg thing, ('_DEBUG' is standard C++)
target_compile_definitions(${PROJECT_NAME} PRIVATE 
  $<$<CONFIG:Debug>:_DEBUG>
  $<$<CONFIG:Debug>:DEBUG=1>
  $<$<CONFIG:Release>:DEBUG=0>
  $<$<CONFIG:Release>:NDEBUG>
)

TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${COREFOUNDATION_LIBRARY} )
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${COCOA_LIBRARY} )
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${CORETEXT_LIBRARY} )
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${OPENGL_LIBRARY} )
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${IMAGEIO_LIBRARY} )
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${COREGRAPHICS_LIBRARY} )
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${ACCELERATE_LIBRARY} )
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} ${QUARTZCORE_LIBRARY} )

TARGET_LINK_LIBRARIES( ${PROJECT_NAME} base )
TARGET_LINK_LIBRARIES( ${PROJECT_NAME} pluginterfaces )

if (MSVC)
    target_compile_options(VST3_Wrapper PRIVATE /Zc:__cplusplus)
endif ()

target_compile_definitions (${PROJECT_NAME} PRIVATE -D_UNICODE -DUNICODE)

if(APPLE)
set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE TRUE)
set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE_EXTENSION vst3)
endif()

# required by Linux build
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

########################### VST3 SDK #######################################

list(APPEND CMAKE_MODULE_PATH "${VST3_SDK}/cmake/modules")

include(SMTG_VST3_SDK)

#---Add base libraries---------------------------
set(VST_SDK TRUE) # used for Base module which provides only a subset of Base for VST-SDK
add_subdirectory(${VST3_SDK}/pluginterfaces ${CMAKE_CURRENT_BINARY_DIR}/pluginterfaces)
add_subdirectory(${VST3_SDK}/base           ${CMAKE_CURRENT_BINARY_DIR}/base)

SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES FOLDER "adaptors")
SET_TARGET_PROPERTIES(base PROPERTIES FOLDER "adaptors")
SET_TARGET_PROPERTIES(pluginterfaces PROPERTIES FOLDER "adaptors")

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES BUNDLE_EXTENSION "gmpi")
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".gmpi")
endif()

if(WIN32 AND SE_LOCAL_BUILD)
    add_custom_command(TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND copy /Y "$(OutDir)$(TargetName)$(TargetExt)" "C:\\Program Files\\Common Files\\SynthEdit\\modules\\$(TargetName)$(TargetExt)"
        COMMENT "Copy to GMPI folder"
        VERBATIM
    )
endif()
