set(_freetype_cflags "-fsanitize=memory -stdlib=libc++ -fsanitize-recover=memory -I${DESTDIR}/usr/local/include -I${DESTDIR}/usr/local/include/c++/v1")
set(_freetype_cxxflags "-fsanitize=memory -stdlib=libc++ -fsanitize-recover=memory -I${DESTDIR}/usr/local/include -I${DESTDIR}/usr/local/include/c++/v1")
set(_freetype_ldflags "-fsanitize=memory -stdlib=libc++ -fsanitize-recover=memory -L${DESTDIR}/usr/local/lib -Wl,-rpath,${DESTDIR}/usr/local/lib")

ExternalProject_Add(dep_freetype
        URL https://download.savannah.gnu.org/releases/freetype/freetype-2.10.1.tar.gz
        URL_HASH SHA256=3a60d391fd579440561bf0e7f31af2222bc610ad6ce4d9d7bd2165bca8669110
        DEPENDS dep_Libcxx
        DOWNLOAD_DIR ${DEP_DOWNLOAD_DIR}/freetype
        BUILD_IN_SOURCE ON
        CONFIGURE_COMMAND  env "CFLAGS=${_freetype_cflags}" "CXXFLAGS=${_freetype_cxxflags}" "LDFLAGS=${_freetype_ldflags}" ./configure "--prefix=${DESTDIR}/usr/local"
        INSTALL_COMMAND make install
            COMMAND     ln -sfn "../local/lib/libfreetype.so" "${DESTDIR}/usr/lib/libfreetype.so"
            COMMAND     ln -sfn "../local/lib/libfreetype.so.6" "${DESTDIR}/usr/lib/libfreetype.so.6"
            COMMAND     ln -sfn "../local/lib/libfreetype.so.6.17.1" "${DESTDIR}/usr/lib/libfreetype.so.6.17.1"
        )
