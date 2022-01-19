set(_mesa3d_cflags "-fsanitize=memory -stdlib=libc++ -fsanitize-recover=memory -fsanitize-blacklist=${CMAKE_CURRENT_LIST_DIR}/ignorelist.txt -I${DESTDIR}/usr/local/include -I${DESTDIR}/usr/local/include/c++/v1")
set(_mesa3d_cxxflags "-fsanitize=memory -stdlib=libc++ -fsanitize-recover=memory -fsanitize-blacklist=${CMAKE_CURRENT_LIST_DIR}/ignorelist.txt -I${DESTDIR}/usr/local/include -I${DESTDIR}/usr/local/include/c++/v1")
set(_mesa3d_ldflags "-fsanitize=memory -stdlib=libc++ -fsanitize-recover=memory -fsanitize-blacklist=${CMAKE_CURRENT_LIST_DIR}/ignorelist.txt -L${DESTDIR}/usr/local/lib -Wl,-rpath,${DESTDIR}/usr/local/lib")
set(_mesa3d_path "${DESTDIR}/usr/local/bin:$ENV{PATH}")

# We need to override PATH variable to make Meson find our build llvm-config and not use system one.

ExternalProject_Add(dep_Mesa3D
        URL https://archive.mesa3d.org/mesa-21.3.3.tar.xz
        URL_HASH SHA256=ad7f4613ea7c5d08d9fcb5025270199e6ceb9aa99fd72ee572b70342240a8121
        DEPENDS dep_Libcxx dep_LLVM dep_Chromium_Libs
        DOWNLOAD_DIR ${DEP_DOWNLOAD_DIR}/Mesa3D
        BUILD_IN_SOURCE ON
        CONFIGURE_COMMAND env "PATH=${_mesa3d_path}" "CFLAGS=${_mesa3d_cflags}" "CXXFLAGS=${_mesa3d_cxxflags}" "LDFLAGS=${_mesa3d_ldflags}" meson build_mesa -Dglx=gallium-xlib -Dgallium-drivers=swrast -Dplatforms=x11 -Ddri3=disabled "-Ddri-drivers=" "-Dvulkan-drivers=" -Db_sanitize=memory -Db_lundef=false -Dgles1=disabled -Dgles2=disabled -Dshared-glapi=disabled "-Dprefix=${DESTDIR}/usr/local" "-Dlibdir=${DESTDIR}/usr/local/lib"
        BUILD_COMMAND     ninja -C build_mesa install
        INSTALL_COMMAND   ln -sfn "../local/lib/libGL.so" "${DESTDIR}/usr/lib/libGL.so"
            COMMAND       ln -sfn "../local/lib/libGL.so.1" "${DESTDIR}/usr/lib/libGL.so.1"
            COMMAND       ln -sfn "../local/lib/libGL.so.1.5.0" "${DESTDIR}/usr/lib/libGL.so.1.5.0"
)