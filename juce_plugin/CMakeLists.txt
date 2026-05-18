cmake_minimum_required(VERSION 3.21)

project(ArtifactRemoverVST3 VERSION 0.1.0 LANGUAGES CXX)

# =========================================================
# JUCE Configuration (only if building standalone)
# =========================================================



if(NOT TARGET juce_add_plugin)
    # Download JUCE if not already present (standalone build)
    include(FetchContent)
    
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.1
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE"
    )
    
    FetchContent_MakeAvailable(JUCE)
endif()

# =========================================================
# VST3 Plugin Target
# =========================================================

juce_add_plugin(ArtifactRemoverVST3
    PRODUCT_NAME "Artifact Remover"
    COMPANY_NAME "ArtifactLab"
    PLUGIN_MANUFACTURER_CODE "ArtL"
    PLUGIN_CODE "ArRm"
    PLUGIN_AU_EXPORT_PREFIX ArtifactRemoverAU
    PLUGIN_AU_MAIN_TYPE "aufl"
    
    FORMATS VST3 Standalone
    
    VST3_CATEGORIES "Analyzer" "Mastering"
    JUCE_VST3_CAN_REPLACE_VST2 FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    DISABLE_AAX_PLUGIN TRUE
    DISABLE_VST2_PLUGIN TRUE
    JUCE_PLUGINHOST_VST FALSE
    JUCE_PLUGINHOST_VST3 TRUE
    # VST3_AUTO_MANIFEST FALSE
)

# =========================================================
# Plugin Source Files
# =========================================================

target_sources(ArtifactRemoverVST3
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
)

# =========================================================
# Plugin Include Directories
# =========================================================

target_include_directories(ArtifactRemoverVST3
    PRIVATE
        .
        ../include/artifact_remover
        ../external/eigen
        ../external/pocketfft
)

# =========================================================
# Link JUCE modules
# =========================================================

target_link_libraries(ArtifactRemoverVST3
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Disable VST2 in JUCE modules
target_compile_definitions(ArtifactRemoverVST3 PRIVATE
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGIN_VST3=1
    JUCE_USE_CURL=0
    
)

# =========================================================
# Compiler Settings
# =========================================================

if(MSVC)
    target_compile_options(ArtifactRemoverVST3 PRIVATE /W4 /fp:fast)
else()
    target_compile_options(ArtifactRemoverVST3 PRIVATE -Wall -Wextra -O3 -ffast-math)
endif()

# =========================================================
# Link artifact_remover library
# =========================================================

# When included as subdirectory, artifact_remover target should exist
if(TARGET artifact_remover)
    target_link_libraries(ArtifactRemoverVST3 
        PRIVATE 
            artifact_remover
            Eigen3::Eigen
    )
else()
    message(WARNING "artifact_remover target not found - plugin may not build correctly")
    message(WARNING "Build from parent directory: cmake --build ../build --config Release")
endif()

