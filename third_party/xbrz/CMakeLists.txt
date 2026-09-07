# Pinned, vendored xBRZ: the renderer filter also works in offline builds.
#
# The filter runs on the CPU every frame the game draws, and this is
# upstream code with nothing for /Od or /RTC1 to catch, so it is built
# optimised in every configuration. Directory scope keeps that away from
# SpriteLib and the rest of the tree. /RTC1 is incompatible with /O2 and
# has to come out with /Od; /Ob0 would otherwise override the inlining
# /O2 turns on.
if(MSVC)
	foreach(_xbrz_cfg "_DEBUG" "_RELWITHDEBINFO" "_MINSIZEREL")
		set(_xbrz_var "CMAKE_CXX_FLAGS${_xbrz_cfg}")
		string(REPLACE "/RTC1" "" ${_xbrz_var} "${${_xbrz_var}}")
		string(REPLACE "/Od"   "" ${_xbrz_var} "${${_xbrz_var}}")
		string(REPLACE "/Ob0"  "" ${_xbrz_var} "${${_xbrz_var}}")
		string(REPLACE "/O1"   "" ${_xbrz_var} "${${_xbrz_var}}")
		set(${_xbrz_var} "${${_xbrz_var}} /O2")
	endforeach()
else()
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O2")
endif()

add_library(darkeden_xbrz STATIC xbrz.cpp)
target_include_directories(darkeden_xbrz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(darkeden_xbrz PUBLIC cxx_std_20)
set_target_properties(darkeden_xbrz PROPERTIES POSITION_INDEPENDENT_CODE ON)
