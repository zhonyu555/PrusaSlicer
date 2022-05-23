#cmake_minimum_required(VERSION 3.13)
#project(PrusaSlicer_Downloader)

add_executable(PrusaSlicer_Downloader  WIN32 
	Downloader.cpp
	Downloader.hpp
	FileGet.cpp
	FileGet.hpp
	FromSlicer/Http.cpp
	FromSlicer/Http.hpp
 )

set_target_properties(PrusaSlicer_Downloader PROPERTIES OUTPUT_NAME "prusa-slicer-downloader")

#encoding_check(PrusaSlicer_Downloader)



target_link_libraries(PrusaSlicer_Downloader libslic3r_gui)


