prusaslicer_add_cmake_project(
    TBB
    # URL "https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.6.0.zip"
    # URL_HASH SHA256=29376a78addbd0a81ac5fca5cc7ef82528281f0659b7e09a07672b136451c7b1
    URL "https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.6.0-rc1.zip"
    URL_HASH SHA256=bdec0bfd995788ed09609d57aa7fb4a1c8ccba7940b10d20c0b3107fcab4c094
    CMAKE_ARGS          
        -DTBB_BUILD_SHARED=OFF
        -DTBB_TEST=OFF
        -DTBB_STRICT=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_DEBUG_POSTFIX=_debug
)

if (MSVC)
    add_debug_dep(dep_TBB)
endif ()


