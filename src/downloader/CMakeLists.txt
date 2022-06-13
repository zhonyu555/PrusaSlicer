#cmake_minimum_required(VERSION 3.13)
#project(PrusaSlicer_Downloader)

#add_executable(PrusaSlicer_Downloader  WIN32
add_executable(PrusaSlicer_Downloader 
	DownloaderApp.cpp
	DownloaderApp.hpp
	Download.cpp
	Download.hpp
	FileGet.cpp
	FileGet.hpp
	InstanceSend.cpp
	InstanceSend.hpp
	FromSlicer/Http.cpp
	FromSlicer/Http.hpp
 )

set_target_properties(PrusaSlicer_Downloader PROPERTIES OUTPUT_NAME "prusa-slicer-downloader")

#encoding_check(PrusaSlicer_Downloader)



target_link_libraries(PrusaSlicer_Downloader libslic3r_gui)


