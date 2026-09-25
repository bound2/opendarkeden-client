set_target_properties(DarkEden PROPERTIES SUFFIX ".mjs")
target_link_options(DarkEden PRIVATE
    # Translation units already use Release optimization. Binaryen -O3 across
    # this large legacy module takes minutes; -O1 keeps incremental links short.
    -O1
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createDarkEden
    -sEXIT_RUNTIME=1
    -sCASE_INSENSITIVE_FS=1
    -sEXPORTED_RUNTIME_METHODS=FS,IDBFS,callMain,ENV
    -lidbfs.js)
foreach(_file index.html launcher.mjs client-config.json)
    configure_file("${CMAKE_SOURCE_DIR}/web/${_file}"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_file}" COPYONLY)
endforeach()
