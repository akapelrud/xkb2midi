cmake_minimum_required(VERSION 2.8)
project(xkb2midi CXX)

#set(AGE_Boost_USE_SYSTEM_LAYOUT ON)
#if(AGE_Boost_USE_SYSTEM_LAYOUT)
	#set(Boost_USE_MULTITHREADED OFF)
#endif()

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

#install(TARGETS ${libraryname} LIBRARY DESTINATION .)

