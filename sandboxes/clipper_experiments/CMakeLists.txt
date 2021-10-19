set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(clipper_experiments ClipperExperiments.cpp)
#set_target_properties(clipper_experiments PROPERTY CXX_STANDARD 20)
#set_target_properties(clipper_experiments PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(clipper_experiments libslic3r)

if (WIN32)
    prusaslicer_copy_dlls(clipper_experiments)
endif()
