# The same pixel oracle runs natively and in a real browser WebGL context.
add_executable(web_sprite_tests
    tests/render/main.cpp tests/render/test_sprite_gpu.cpp tests/render/benchmark.cpp
    tests/framework/test_framework.cpp)
target_include_directories(web_sprite_tests PRIVATE tests/framework)
target_link_libraries(web_sprite_tests PRIVATE SpriteLib darkeden_xbrz)
set_target_properties(web_sprite_tests PROPERTIES SUFFIX ".html")
target_link_options(web_sprite_tests PRIVATE -sASSERTIONS=1 -sEXIT_RUNTIME=1)
