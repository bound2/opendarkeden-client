cmake_minimum_required(VERSION 3.21)

#----------------------------------------------------------------------
# Unit tests
#----------------------------------------------------------------------
#
# Only code compiled into a static library can be linked into a test
# binary. Today that means basic/, framelib/ (header-only templates),
# SpriteLib/, DXLib/, TextSystem/, VS_UI and packetwire (the whole wire
# layer: streams, info classes, every packet class and the factory
# tables - docs/RESTRUCTURING.md task 2.4). Game logic that is compiled
# straight into the DarkEden executable, including the packet handlers
# under Client/PacketHandler, is not reachable from here.
#
#----------------------------------------------------------------------

add_library(test_framework STATIC
	framework/test_framework.cpp
)

target_include_directories(test_framework PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/framework
)

#----------------------------------------------------------------------
# Test sources are globbed so adding a test only means adding a file to
# unit/, matching how the rest of this project collects sources.
#----------------------------------------------------------------------
file(GLOB UNIT_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp)

#----------------------------------------------------------------------
# There is no stubs/ directory any more (docs/RESTRUCTURING.md task
# 5.3). It held the globals and functions a linked library referenced
# but the DarkEden executable defined, and every one of them turned out
# to be a seam worth cutting rather than filling. Of the libraries this
# binary links, g_pLast was reached only by TextService::RenderText,
# which draws through the back buffer and has ~45 live call sites; it
# is defined in Client/TextServiceScreen.cpp now, on the executable
# side of the split. SendBugReport moved into packetwire in 5.1.
# (VS_UI reaches g_pLast too, and is not linked here.)
# A library that needs
# something from the program around it asks through a host struct - see
# MItemHost, MPriceHost, WireHost - which a test can install, rather
# than through a symbol a test binary has to invent.
#----------------------------------------------------------------------
add_executable(unit_tests
	framework/test_main.cpp
	${UNIT_TEST_SOURCES}
)

target_link_libraries(unit_tests PRIVATE
	test_framework
	basic
	SpriteLib
	TextSystem
	packetwire
	gamemodel
)

target_include_directories(unit_tests PRIVATE
	${CMAKE_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/basic
	${CMAKE_SOURCE_DIR}/Client/framelib
	${CMAKE_SOURCE_DIR}/Client/TextSystem
	# Test-only seams into the packetwire streams (support/packet_stream_access.h)
	${CMAKE_CURRENT_SOURCE_DIR}/support
)

#----------------------------------------------------------------------
# Every TU that shares class definitions with the packetwire library
# (which compiles with __GAME_CLIENT__=1 and the Windows wire macros)
# must see the identical definitions - Packet's virtual set changes
# under these macros, so a mismatch would be a real ODR/vtable break,
# not a style issue. One list, used by every such TU below, so the two
# blocks cannot drift apart.
#----------------------------------------------------------------------
set(PACKETWIRE_TEST_DEFINITIONS "__GAME_CLIENT__=1")
if(WIN32)
	list(APPEND PACKETWIRE_TEST_DEFINITIONS "__WIN32__" "__WINDOWS__")
endif()

#----------------------------------------------------------------------
# Wire-layout inventory (unit/test_wire_layout.cpp).
#
# That one source constructs every packet factory under Client/Packet
# through tests/generated/WireInventory.inc, the registry that
# tests/tools/gen_wire_inventory.pl writes (one include and one
# registration per factory class; the wire_inventory_fresh test below
# pins it). Its include dirs are packetwire's PUBLIC ones plus the
# generated directory, and it needs the packetwire defines so the class
# definitions match the library's (docs/RESTRUCTURING.md task 2.5).
#----------------------------------------------------------------------
set_source_files_properties(
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_wire_layout.cpp
	PROPERTIES
		INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/generated"
		COMPILE_DEFINITIONS "${PACKETWIRE_TEST_DEFINITIONS};WIRE_LAYOUT_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/wire-layout.txt\""
)

#----------------------------------------------------------------------
# packetwire tests. Include dirs come from packetwire's PUBLIC dirs.
#----------------------------------------------------------------------
set_source_files_properties(
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_packetwire_parsers.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_packet_dispatcher.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_packet_factories.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_packet_goldens.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_packet_span_io.cpp
	# The framing entry point, SocketOutputStream::write(const Packet*):
	# what the ring and the sequence counter hold after a body write
	# throws. Defines its own Packet subclass, so it needs the same
	# definitions - Packet's virtual set changes under them.
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_output_stream_rollback.cpp
	# The second span/typed-scalar slice: the CGSkillTo* family's fields
	# and its truncated-input contract, over the same stream classes.
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_packet_skill_family_wire.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_wire_scalar.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_stringstream.cpp
	# The character overloads and the diagnostic texts built out of them
	# (Throwable, __assert__), which is why this one reaches Exception.h
	# and PacketAssert.h as well.
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_stringstream_chars.cpp
	# gamemodel compiles with the same definitions: its headers reach
	# Client/Packet/Exception.h, whose inline bodies switch on __WIN32__,
	# and Client/Packet/Types/CreatureTypes.h, which switches on
	# __GAME_CLIENT__. Every test that includes a Client/ header needs an
	# entry here.
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_gamemodel_tables.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_item_table.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_item_core.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_item_containers.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_inventory_storage_shop.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_trade_manager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_price_manager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_player_gear.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_skill_core.cpp
	# The C++20 container/string helper slice spans both libraries: the
	# packet and Properties sites and the gamemodel manager sites.
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_cpp20_container_helpers.cpp
	# The second slice of the same backlog item: the skill domain's step
	# map and step lists, the two line-parsing loaders and the two
	# availability filter scans. It includes the four Client/ headers it
	# tests, and gamemodel_world.h besides, so it needs the same
	# definitions.
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_cpp20_container_helpers_2.cpp
	# The second source_location slice. It expands Assert() itself, and
	# Assert() is conditioned on __WIN32__ - without these definitions the
	# macro would fall back to <assert.h>, a branch the library never
	# compiles, and the test would pin nothing. (__END_CATCH, which it
	# also expands, is conditioned on NDEBUG alone.)
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_packet_source_location.cpp
	# PacketIDSet::deletePacketID. It names CPS_NONE, which PlayerStatus.h
	# declares only under __GAME_CLIENT__, and Packet.h conditions a member
	# on the same macro, so the TU must see the definitions packetwire was
	# built with.
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_packet_id_set.cpp
	# What SendBugReport actually sends. It builds a CGSay and hands it
	# to a Player subclass, so the Packet vtable and the CGSay class
	# definition must be the library's.
	${CMAKE_CURRENT_SOURCE_DIR}/unit/test_bug_report_message.cpp
	PROPERTIES
		COMPILE_DEFINITIONS "${PACKETWIRE_TEST_DEFINITIONS};WIRE_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/golden\""
)

add_test(NAME unit_tests COMMAND unit_tests)

#----------------------------------------------------------------------
# Shrink-only ratchets (tests/ratchet/ratchets.sh) - fails when a debt
# metric rises AND when it falls unrecorded (tighten the baseline in
# the same commit). Needs bash (Git for Windows ships one); without it
# the check is skipped, not passed.
#----------------------------------------------------------------------
if(REQUIRE_TEST_TOOLS)
	find_program(BASH_EXECUTABLE bash REQUIRED)
else()
	find_program(BASH_EXECUTABLE bash)
endif()
if(BASH_EXECUTABLE)
	add_test(
		NAME ratchets
		COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/ratchet/ratchets.sh ${CMAKE_BINARY_DIR}
	)
else()
	message(STATUS "bash not found: the ratchets test is not registered")
endif()

#----------------------------------------------------------------------
# Include-graph architecture rules (tests/arch/check_includes.pl) -
# walks the packetwire quote-include closure and fails on forbidden
# edges (W1: only Client/Packet, basic, Client_PCH.h; W2: no
# exe-defined debug facilities). Skipped without perl, not passed.
# find_program lives here, above every block that tests the variable -
# it used to sit further down, which would leave any earlier test
# silently unregistered on the first configure of a fresh tree.
#----------------------------------------------------------------------
if(REQUIRE_TEST_TOOLS)
	find_program(PERL_EXECUTABLE perl REQUIRED)
else()
	find_program(PERL_EXECUTABLE perl)
endif()
if(PERL_EXECUTABLE)
	add_test(
		NAME arch_includes
		COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/arch/check_includes.pl
	)
else()
	message(STATUS "perl not found: the arch_includes test is not registered")
endif()

#----------------------------------------------------------------------
# Format-arity audit (tests/tools/check_format_arity.pl) - the check the
# checked formatter cannot make for itself.
#
# SafeFormat::Format makes a data-file format string safe, but it cannot
# tell that the entry MEANS what its call site passes: an entry with one
# %s more than the call site hands over is no longer a memory-safety
# defect, but it now prints a literal "%s" in the user interface. This
# compares every converted site against the built-in English table -
# which is not a sample, since InitGameStringTable installs it over the
# file data on the English path. Needs perl (found above); without it
# the check is skipped, not passed.
#----------------------------------------------------------------------
if(PERL_EXECUTABLE)
	add_test(
		NAME format_arity
		COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_format_arity.pl
	)
else()
	message(STATUS "perl not found: the format_arity test is not registered")
endif()

#----------------------------------------------------------------------
# Packet-index audit (tests/tools/check_packet_indices.pl) - the other
# half of code-health priority 1.
#
# Ratchet R3 covers the lengths a server sends into a copy. This covers
# the indices it sends into a subscript, and it walks the VALUE rather
# than the spelling: the population is not `array[pPacket->getSlotID()]`
# but a local assigned from it and used twenty lines later. Handlers are
# executable-side, so this is the only kind of check they can have.
#
# The repo root is passed so the script does not have to guess it from
# its own location. Needs perl (found above); without it the check is
# skipped, not passed.
#----------------------------------------------------------------------
if(PERL_EXECUTABLE)
	add_test(
		NAME packet_indices
		COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_packet_indices.pl ${CMAKE_SOURCE_DIR}
	)
else()
	message(STATUS "perl not found: the packet_indices test is not registered")
endif()

#----------------------------------------------------------------------
# The committed WireInventory.inc must match a fresh generation, so a
# new or changed factory cannot go unconstructed and uninventoried. Needs perl (Git for
# Windows ships one, found above); without it the check is skipped,
# not passed.
#----------------------------------------------------------------------
if(PERL_EXECUTABLE)
	add_test(
		NAME wire_inventory_fresh
		COMMAND ${CMAKE_COMMAND}
			-DPERL_EXECUTABLE=${PERL_EXECUTABLE}
			-DSOURCE_DIR=${CMAKE_SOURCE_DIR}
			-P ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_wire_inventory_fresh.cmake
	)
else()
	message(STATUS "perl not found: the wire_inventory_fresh test is not registered")
endif()
