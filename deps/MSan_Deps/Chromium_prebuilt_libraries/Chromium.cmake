if (DEP_MSAN_ORIGIN_TRACKING)
    set(chromium_url https://www.googleapis.com/download/storage/v1/b/chromium-browser-msan/o/linux-release%2Fmsan-chained-origins-linux-release-812431.zip?alt=media)
    set(chromium_url_hash b99c3e50ab936e44f6e5b4e28b7f8bab0b01912bcb068af258f366fe1e18a6f6)
else ()
    set(chromium_url https://www.googleapis.com/download/storage/v1/b/chromium-browser-msan/o/linux-release%2Fmsan-no-origins-linux-release-812433.zip?alt=media)
    set(chromium_url_hash cfacfe4c2c3ee284015b74ada493cb992d4f7ac82e6325d27384d18437738c4c)
endif ()

ExternalProject_Add(dep_Chromium_Libs
        URL ${chromium_url}
        URL_HASH SHA256=${chromium_url_hash}
        DOWNLOAD_DIR ${DEP_DOWNLOAD_DIR}/Chromium_Libs
        BUILD_IN_SOURCE ON
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ${CMAKE_COMMAND} "-D_MSAN_COPY_FROM=./instrumented_libraries_prebuilt/msan/lib" "-D_MSAN_COPY_TO=${DESTDIR}/usr/lib" -P ${CMAKE_CURRENT_LIST_DIR}/copy_chromium_prebuilt_libraries.cmake
        )
