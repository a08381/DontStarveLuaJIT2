# Windows x64 function-entry patches based on executable signatures.
ds_add_dynamic_plugin(plugin_skins_verify_patch
    plugin_skins_verify_patch.cpp)
target_link_libraries(plugin_skins_verify_patch PRIVATE
    spdlog::spdlog
    function_relocation)
target_compile_definitions(plugin_skins_verify_patch PRIVATE
    SPDLOG_COMPILED_LIB
    SPDLOG_FMT_EXTERNAL)
target_include_directories(plugin_skins_verify_patch PRIVATE
    ${PROJECT_SOURCE_DIR}/3rd
    ${FRIDA_GUM_INCLUDE_DIR}
    ${FUNCTION_RELOCATION_INCLUDE_DIR})
