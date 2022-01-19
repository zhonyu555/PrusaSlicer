function(copy_chromium_prebuilt_libraries path_from path_to)
    if (NOT ${ARGC} EQUAL 2)
        message(FATAL_ERROR "Invalid argument count.")
    endif()

    set(chromium_libs_list
            libatk-1.0
            libatk-bridge-2.0
            libatspi
            libcairo
            libcairo-gobject
            libcairo-script-interpreter
            libdbus-1
            libdbus-glib-1
            libdbusmenu-glib
            libdbusmenu-gtk
            libdbusmenu-gtk3
            libdbusmenu-jsonloader
            libffi
            libfreetype
            libgdk-3
            libgdk_pixbuf-2.0
            libgdk_pixbuf_xlib-2.0
            libgio-2.0
            libglib-2.0
            libgobject-2.0
            libgtk-3
            libharfbuzz
            libharfbuzz-gobject
            libharfbuzz-icu
            libjasper
            libpango-1.0
            libpangocairo-1.0
            libpangoft2-1.0
            libpangoxft-1.0
            libpixman-1
            libpng12
            libsecret-1
            libudev
            libX11
            libX11-xcb
            libXau
            libxcb
            libXcomposite
            libXcursor
            libXdamage
            libXdmcp
            libXext
            libXfixes
            libXinerama
            libXi
            libXrandr
            libXrender
            libXss
            libXtst)

    set(chromium_libs_dirs_list
            cairo
            dbus-1.0
            glib-2.0
            gtk-2.0
            gtk-3.0
            udev)

    foreach(chromium_lib ${chromium_libs_list})
        file(GLOB chromium_lib_files "${path_from}/${chromium_lib}*")
        if (NOT chromium_lib_files)
            message(FATAL_ERROR "Library \"${chromium_lib}\" was not found.")
        endif()
        foreach(chromium_lib_file ${chromium_lib_files})
            file(COPY ${chromium_lib_file} DESTINATION ${path_to})
            message("Copying file: ${chromium_lib_file}.")
        endforeach()
    endforeach()

    foreach(chromium_lib_dir ${chromium_libs_dirs_list})
        file(GLOB lib_directory "${path_from}/${chromium_lib_dir}")
        if (NOT lib_directory)
            message(FATAL_ERROR "Library directory \"${chromium_lib_dir}\" was not found.")
        endif()
        file(COPY ${lib_directory} DESTINATION ${path_to})
        message("Copying directory: ${lib_directory}.")
    endforeach()
endfunction()

copy_chromium_prebuilt_libraries(${_MSAN_COPY_FROM} ${_MSAN_COPY_TO})
