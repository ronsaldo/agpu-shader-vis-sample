# AbstractGPU cmake build script.
cmake_minimum_required(VERSION 3.1)
project(AgpuShaderVisSample)

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/dist")

set(MainOutputDirectory "${AgpuShaderVisSample_BINARY_DIR}/dist")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${MainOutputDirectory} )
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${MainOutputDirectory} )
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${MainOutputDirectory} )

# For VisualStudio in Window
find_path(SDL2_INCLUDE_DIRS
  NAMES SDL.h
  PATH_SUFFIXES SDL2
)

find_path(SDL2_LIBRARY_PATH
  NAMES SDL2.lib SDL2.a SDL2-2.0.lib SDL2-2.0.a libSDL2-2.0.a
)

find_library(SDL2_LIBRARY NAMES SDL2-2.0 SDL2 PATHS ${SDL2_LIBRARY_PATH})
find_library(SDL2_MAIN_LIBRARY NAMES SDL2main PATHS ${SDL2_LIBRARY_PATH})

if(SDL2_INCLUDE_DIRS AND SDL2_LIBRARY AND SDL2_INCLUDE_DIRS)
    # Use the SDL2 include path.
    include_directories(${SDL2_INCLUDE_DIRS})

	set(SDL2_FOUND TRUE)
	if(SDL2_MAIN_LIBRARY)
    	set(SDL2_LIBRARIES ${SDL2_LIBRARY} ${SDL2_MAIN_LIBRARY})
	else()
		set(SDL2_LIBRARIES ${SDL2_LIBRARY})
	endif()
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/abstract-gpu/include")

add_subdirectory(thirdparty)

add_subdirectory(apps)

