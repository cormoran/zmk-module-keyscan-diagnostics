# ZMK Module - Keyscan Diagnostics CMakeLists.txt

# Include directories
zephyr_include_directories(include)

# Helper function to set up nanopb and proto compilation
macro(setup_nanopb_proto)
    if(NOT NANOPB_INITIALIZED)
        list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
        include(nanopb)
        set(NANOPB_GENERATE_CPP_APPEND_PATH TRUE)
        set(NANOPB_GENERATE_CPP_STANDALONE OFF)
        set(NANOPB_INITIALIZED TRUE)
        
        # NOTE: adding to app target directly causes build issues, so we create a library instead
        zephyr_library()
        file(GLOB_RECURSE PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto)
        
        nanopb_generate_cpp(proto_srcs proto_hdrs RELPATH ${CMAKE_CURRENT_SOURCE_DIR} ${PROTO_FILES})
        target_include_directories(${ZEPHYR_CURRENT_LIBRARY} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
        target_sources(${ZEPHYR_CURRENT_LIBRARY} PRIVATE ${proto_srcs} ${proto_hdrs})
        
        # app should depend on the generated proto files
        target_include_directories(app PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/proto)
        add_dependencies(app ${ZEPHYR_CURRENT_LIBRARY})
    endif()
endmacro()

# Template Feature (legacy - for backward compatibility)
if(CONFIG_ZMK_TEMPLATE_FEATURE)
    if(CONFIG_ZMK_TEMPLATE_FEATURE_STUDIO_RPC)
        target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/studio/custom_handler.c)
        setup_nanopb_proto()
    endif()
endif()

# Keyscan Diagnostics Feature
if(CONFIG_ZMK_KEYSCAN_DIAGNOSTICS)
    if(CONFIG_ZMK_KEYSCAN_DIAGNOSTICS_STUDIO_RPC)
        target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/studio/keyscan_diagnostics_handler.c)
        setup_nanopb_proto()
    endif()
endif()
