cmake_minimum_required(VERSION 3.10)
project(VoyeSeq)

# 1. Define DPF Location
# Pointing to the root 'dpf' folder (not src/dpf)
set(DPF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dpf")
list(APPEND CMAKE_MODULE_PATH "${DPF_DIR}/cmake")
include(DPF-plugin)

# 2. Force Global Include
# This tells DPF where to find your DistrhoPluginInfo.h
include_directories(src)

# 3. FIX FOR ARDOUR 9 
# (Must be placed before dpf_add_plugin)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,nodelete")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-z,nodelete")

# 4. Add DPF to the build
add_subdirectory("${DPF_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/distrho" EXCLUDE_FROM_ALL)

# 5. Define your source files (Paths are relative to CMakeLists.txt)
set(PLUGIN_DSP_FILES
    src/VoyeSeqPlugin.cpp
)

set(PLUGIN_UI_FILES
    src/VoyeSeqUI.cpp
    src/VoyeRenderer.cpp
)

# 6. Add the Plugin
dpf_add_plugin(VoyeSeq
    TARGETS jack lv2
    UI_TYPE opengl       # <-- REQUIRED: Do not comment this out for NanoVG!
    FILES_DSP ${PLUGIN_DSP_FILES}
    FILES_UI  ${PLUGIN_UI_FILES}
)

# (Optional: Uncomment these if VoyeSeq needs to save/load presets)
# target_compile_definitions(VoyeSeq PUBLIC 
#     DISTRHO_PLUGIN_WANT_STATE=1
#     DISTRHO_PLUGIN_WANT_FULL_STATE=1
# )