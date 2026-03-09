cmake_minimum_required(VERSION 3.10)
project(VoyeSeq)

# 1. Framework Path
set(DPF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/distrho")

# 2. Add DPF Modules
list(APPEND CMAKE_MODULE_PATH "${DPF_DIR}/cmake")
include(DPF-plugin)

# 3. Add DPF Subdirectory
add_subdirectory("${DPF_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/distrho" EXCLUDE_FROM_ALL)

# 4. GLOBAL INCLUDE (The Fix)
# We use include_directories instead of target_include_directories
# and place it BEFORE dpf_add_plugin so all internal targets see it.
include_directories(src)

# 5. Define the Plugin
# Named arguments are the standard for modern DPF
dpf_add_plugin(VoyeSeq
    TARGETS jack lv2
    FILES_DSP 
        src/VoyeSeqPlugin.cpp
    FILES_UI 
        src/VoyeSeqUI.cpp
    UI_TYPE opengl
)
