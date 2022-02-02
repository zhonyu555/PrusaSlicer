function(install_msan_shared_libraries path_from path_to)
    if (NOT ${ARGC} EQUAL 2)
        message(FATAL_ERROR "Invalid argument count.")
    endif()

    set(msan_libs_list
            libc++
            libc++abi
            libfontconfig
            libfreetype
            libGL
            libLLVM)

    foreach(msan_lib ${msan_libs_list})
        file(GLOB msan_lib_files "${path_from}/${msan_lib}*")
        if (NOT msan_lib_files)
            message(FATAL_ERROR "Library \"${msan_lib}\" was not found.")
        endif()
        foreach(msan_lib_file ${msan_lib_files})
            file(COPY ${msan_lib_file} DESTINATION ${path_to})
            message("Copying file: ${msan_lib_file}.")
        endforeach()
    endforeach()

    foreach(msan_lib_dir ${msan_libs_dirs_list})
        file(GLOB lib_directory "${path_from}/${msan_lib_dir}")
        if (NOT lib_directory)
            message(FATAL_ERROR "Library directory \"${msan_lib_dir}\" was not found.")
        endif()
        file(COPY ${lib_directory} DESTINATION ${path_to})
        message("Copying directory: ${lib_directory}.")
    endforeach()
endfunction()

install_msan_shared_libraries(${_MSAN_COPY_FROM} ${_MSAN_COPY_TO})
