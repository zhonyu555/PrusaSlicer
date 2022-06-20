#cmake_minimum_required(VERSION 3.13)
#project(PrusaSlicer_Downloader)

set(SLIC3R_DOWNLOADER_SOURCES
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

if (APPLE)
    list(APPEND SLIC3R_DOWNLOADER_SOURCES
           InstanceSendMac.mm
	     InstanceSendMac.h
        )
endif ()

if(NOT WIN32)
	add_executable(PrusaSlicer_Downloader ${SLIC3R_DOWNLOADER_SOURCES})
	set_target_properties(PrusaSlicer_Downloader PROPERTIES OUTPUT_NAME "prusa-slicer-downloader")
	target_link_libraries(PrusaSlicer_Downloader libslic3r_gui)
else()
	add_executable(PrusaSlicer_Downloader WIN32 ${SLIC3R_DOWNLOADER_SOURCES})
	set_target_properties(PrusaSlicer_Downloader PROPERTIES OUTPUT_NAME "prusa-slicer-downloader")
	target_link_libraries(PrusaSlicer_Downloader libslic3r_gui)
endif()



#encoding_check(PrusaSlicer_Downloader)






