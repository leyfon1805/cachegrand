find_program(CCACHE_FOUND_PATH ccache)
if(CCACHE_FOUND_PATH)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_FOUND_PATH})
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_FOUND_PATH})

    message(STATUS "Found ccache: ${CCACHE_FOUND_PATH} (ccache dir $ENV{CCACHE_DIR})")
endif(CCACHE_FOUND_PATH)
