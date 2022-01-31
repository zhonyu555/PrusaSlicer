
ExternalProject_Add(dep_iconv
    EXCLUDE_FROM_ALL ON
    URL "https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.16.tar.gz"
    URL_HASH SHA256=e6a1b1b589654277ee790cce3734f07876ac4ccfaecbee8afa0b649cf529cc04
    DOWNLOAD_DIR ${DEP_DOWNLOAD_DIR}/iconv
    BUILD_IN_SOURCE ON
    CONFIGURE_COMMAND ./configure
        "--prefix=${DESTDIR}/usr/local"
        "${MSAN_CMAKE_C_FLAGS}"
        "${MSAN_CMAKE_LD_FLAGS}"
    BUILD_COMMAND  make "-j${NPROC}"
    INSTALL_COMMAND make install
)
