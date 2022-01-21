set(_fontconfig_cflags "-fsanitize=memory -stdlib=libc++ -fsanitize-recover=memory -I${DESTDIR}/usr/local/include -I${DESTDIR}/usr/local/include/c++/v1")
set(_fontconfig_cxxflags "-fsanitize=memory -stdlib=libc++ -fsanitize-recover=memory -I${DESTDIR}/usr/local/include -I${DESTDIR}/usr/local/include/c++/v1")
set(_fontconfig_ldflags "-fsanitize=memory -stdlib=libc++ -fsanitize-recover=memory -L${DESTDIR}/usr/local/lib -Wl,-rpath,${DESTDIR}/usr/local/lib")

ExternalProject_Add(dep_fontconfig
        URL https://www.freedesktop.org/software/fontconfig/release/fontconfig-2.13.1.tar.gz
        URL_HASH SHA256=9f0d852b39d75fc655f9f53850eb32555394f36104a044bb2b2fc9e66dbbfa7f
        DEPENDS dep_Libcxx ${EXPAT_PKG}
        DOWNLOAD_DIR ${DEP_DOWNLOAD_DIR}/fontconfig
        BUILD_IN_SOURCE ON
        CONFIGURE_COMMAND  env "CFLAGS=${_fontconfig_cflags}" "CXXFLAGS=${_fontconfig_cxxflags}" "LDFLAGS=${_fontconfig_ldflags}" ./configure --disable-docs --disable-static "--sysconfdir=/etc" "--localstatedir=/var" "--prefix=${DESTDIR}/usr/local"
        INSTALL_COMMAND make install-exec && cd fontconfig && make install-fontconfigincludeHEADERS
            COMMAND     ln -sfn "../local/lib/libfontconfig.so" "${DESTDIR}/usr/lib/libfontconfig.so"
            COMMAND     ln -sfn "../local/lib/libfontconfig.so.1" "${DESTDIR}/usr/lib/libfontconfig.so.1"
            COMMAND     ln -sfn "../local/lib/libfontconfig.so.1.12.0" "${DESTDIR}/usr/lib/libfontconfig.so.1.12.0"
        )
