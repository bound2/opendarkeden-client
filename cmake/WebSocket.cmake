if(EMSCRIPTEN)
    target_link_options(packetwire PUBLIC -lwebsocket.js)
else()
    include(FetchContent)
    # Pin the implementation, including its TLS verification behavior. Disable
    # compression: the existing binary protocol does not need another codec.
    set(USE_TLS ON)
    set(USE_ZLIB OFF)
    set(IXWEBSOCKET_INSTALL OFF)
    if(NOT APPLE)
        set(USE_OPEN_SSL ON)
    endif()
    FetchContent_Declare(ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        GIT_TAG 64fae7676bd8fe31f7cb4bcde7a6841892dad65e
        GIT_CONFIG core.symlinks=false)
    FetchContent_MakeAvailable(ixwebsocket)
    target_link_libraries(packetwire PRIVATE ixwebsocket::ixwebsocket)
endif()

if(BUILD_TESTS OR EMSCRIPTEN)
    add_executable(transport_tests tests/transport/main.cpp)
    target_link_libraries(transport_tests PRIVATE packetwire)
    if(EMSCRIPTEN)
        set_target_properties(transport_tests PROPERTIES SUFFIX ".mjs")
        target_link_options(transport_tests PRIVATE -O1 -sMODULARIZE=1 -sEXPORT_ES6=1
            -sEXIT_RUNTIME=1 -sEXPORTED_RUNTIME_METHODS=callMain)
    endif()
endif()
