cmake_minimum_required(VERSION 2.8)
project(xkb2midi CXX)

find_package(Boost 1.46 COMPONENTS program_options REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

add_definitions(-std=c++11)

add_executable(xkb2midi
	#${conditional_sources}
	main.cpp
	midiprocess.cpp
	)

target_link_libraries(xkb2midi
	${Boost_LIBRARIES}
	X11
	jack
	)
