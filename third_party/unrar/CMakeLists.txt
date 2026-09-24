# Pinned UnRAR 7.3.1 sources; see README.md for the portable-access patch.
# Explicit library source list from upstream's makefile OBJECTS + LIB_OBJ.
add_library(darkeden_unrar STATIC
	rar.cpp strlist.cpp strfn.cpp pathfn.cpp smallfn.cpp global.cpp file.cpp
	filefn.cpp filcreat.cpp archive.cpp arcread.cpp unicode.cpp system.cpp
	crypt.cpp crc.cpp rawread.cpp encname.cpp resource.cpp match.cpp timefn.cpp
	rdwrfn.cpp consio.cpp options.cpp errhnd.cpp rarvm.cpp secpassword.cpp
	rijndael.cpp getbits.cpp sha1.cpp sha256.cpp blake2s.cpp hash.cpp extinfo.cpp
	extract.cpp volume.cpp list.cpp find.cpp unpack.cpp headers.cpp threadpool.cpp
	rs16.cpp cmddata.cpp ui.cpp largepage.cpp filestr.cpp scantree.cpp dll.cpp qopen.cpp
)
target_include_directories(darkeden_unrar PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(darkeden_unrar PRIVATE RARDLL UNRAR SILENT)
set_target_properties(darkeden_unrar PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(WIN32)
	# Windows-only objects in upstream's UnRARDll.vcxproj.
	target_sources(darkeden_unrar PRIVATE isnt.cpp motw.cpp)
	target_link_libraries(darkeden_unrar PRIVATE shlwapi powrprof psapi wbemuuid
		advapi32 ole32 oleaut32)
elseif(EMSCRIPTEN)
	# Browser asset reads run on the main thread; spawning UnRAR workers would
	# require SharedArrayBuffer and cross-origin isolation for every deployment.
	target_compile_definitions(darkeden_unrar PRIVATE _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE)
else()
	find_package(Threads REQUIRED)
	target_compile_definitions(darkeden_unrar PRIVATE
		_FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE RAR_SMP)
	target_link_libraries(darkeden_unrar PRIVATE Threads::Threads)
endif()
install(FILES license.txt acknow.txt README.md DESTINATION share/licenses/unrar COMPONENT unrar)
install(TARGETS darkeden_unrar ARCHIVE DESTINATION lib COMPONENT unrar)
