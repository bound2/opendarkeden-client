cmake_minimum_required(VERSION 3.20)

# FrameLib - Frame/Sprite animation library

# Collect source files
file(GLOB FRAMELIB_SOURCES CONFIGURE_DEPENDS *.cpp)

# Remove precompiled header source
list(FILTER FRAMELIB_SOURCES EXCLUDE REGEX ".*_PCH\\.cpp")

# Create library
add_library(framelib STATIC ${FRAMELIB_SOURCES})

# Include directories
# NOTE: Do not add ../Packet here. Client/Packet/fileapi.h shadows the real
# Windows SDK <fileapi.h> that <windows.h> depends on internally, and
# framelib does not include anything from Packet.
target_include_directories(framelib PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../..
	${CMAKE_CURRENT_SOURCE_DIR}/../SpriteLib
)

# Link libraries
target_link_libraries(framelib PUBLIC basic SpriteLib)
