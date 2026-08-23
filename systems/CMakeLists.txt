# Select exactly one target runner. Platform-specific source lists and link
# dependencies remain in that runner's directory.
if (NEO1_PLATFORM STREQUAL "pico")
	add_subdirectory(neo1-pico)
elseif (NEO1_PLATFORM STREQUAL "sdl")
	add_subdirectory(neo1-sdl)
else()
	message(FATAL_ERROR "Unsupported NEO1_PLATFORM: ${NEO1_PLATFORM}")
endif()
