cmake_minimum_required(VERSION 2.8.11) 

project(AvTranscoder)

# Define AvTranscoder default path to profiles
add_definitions(-DAVTRANSCODER_DEFAULT_AVPROFILES="${CMAKE_INSTALL_PREFIX}/share/avprofiles")

# Diplay commands being ran by CMake
set(CMAKE_VERBOSE_MAKEFILE OFF)

# The install rule does not depend on all, i.e. everything will not be built before installing
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)

# CPP flags on debug / release mode
if(MSVC)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
else()
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -fPIC -g")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -fPIC -O3")
endif()

# CPP flag to create code coverage report
if(AVTRANSCODER_COVERAGE)
	message("Add coverage build option.")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
endif()

# Build library
add_subdirectory(src)

# Build apps
if(AVTRANSCODER_DISABLE_APPS)
	message("Apps disabled, will not build applications.")
else()
	add_subdirectory(app)
endif()
