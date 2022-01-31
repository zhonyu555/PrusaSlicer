set(_fontconfig_cflags "-fsanitize=memory -fsanitize-recover=memory -I${DESTDIR}/usr/local/include -I${DESTDIR}/usr/local/include/c++/v1")
set(_fontconfig_cxxflags "-fsanitize=memory -nostdinc++ -fsanitize-recover=memory -I${DESTDIR}/usr/local/include -I${DESTDIR}/usr/local/include/c++/v1")
set(_fontconfig_ldflags "-fsanitize=memory -fsanitize-recover=memory -Wl,-rpath,${DESTDIR}/usr/local/lib,-L${DESTDIR}/usr/local/lib,-lc++")

ExternalProject_Add(dep_fontconfig
        URL https://www.freedesktop.org/software/fontconfig/release/fontconfig-2.13.1.tar.gz
        URL_HASH SHA256=9f0d852b39d75fc655f9f53850eb32555394f36104a044bb2b2fc9e66dbbfa7f
        DEPENDS dep_Libcxx dep_LLVM dep_Mesa3D
        DOWNLOAD_DIR ${DEP_DOWNLOAD_DIR}/fontconfig
        BUILD_IN_SOURCE ON
        CONFIGURE_COMMAND  env "CFLAGS=${_fontconfig_cflags}" "CXXFLAGS=${_fontconfig_cxxflags}" "LDFLAGS=${_fontconfig_ldflags}" ./configure --disable-docs --disable-static "--sysconfdir=/etc" "--localstatedir=/var" "--prefix=${DESTDIR}/usr/local"
        INSTALL_COMMAND make install-exec && cd fontconfig && make install-fontconfigincludeHEADERS
        )
