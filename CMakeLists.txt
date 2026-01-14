cmake_minimum_required(VERSION 3.10)
project(VoyeSeq)

set(DPF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/dpf")
list(APPEND CMAKE_MODULE_PATH "${DPF_DIR}/cmake")
include(DPF-plugin)

# 1. FORCE GLOBAL INCLUDE (The Fix)
# This ensures DistrhoPluginInfo.h is found by DPF's internal checks
include_directories(src)

# 2. ADD DPF
add_subdirectory("${DPF_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/distrho" EXCLUDE_FROM_ALL)

# 3. DEFINE FILES
set(PLUGIN_DSP_FILES
    src/VoyeSeqPlugin.cpp
)

set(PLUGIN_UI_FILES
    src/VoyeSeqUI.cpp
    src/VoyeRenderer.cpp
)

# 4. ADD PLUGIN
dpf_add_plugin(VoyeSeq
    TARGETS jack lv2
    FILES_DSP ${PLUGIN_DSP_FILES}
    FILES_UI  ${PLUGIN_UI_FILES}
#    UI_TYPE opengl
)

#target_compile_definitions(VoyeSeq PUBLIC 
#    DISTRHO_PLUGIN_WANT_STATE=1
#    DISTRHO_PLUGIN_WANT_FULL_STATE=1
#)
