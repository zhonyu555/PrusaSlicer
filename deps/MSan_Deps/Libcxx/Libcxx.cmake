set(_build_j "-j${NPROC}")
set(_build_j "-j${NPROC}")
ExternalProject_Add(dep_Libcxx
        URL https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.1/llvm-project-12.0.1.src.tar.xz
        URL_HASH SHA256=129cb25cd13677aad951ce5c2deb0fe4afc1e9d98950f53b51bdcfb5a73afa0e
        EXCLUDE_FROM_ALL ON
        INSTALL_DIR      ${DESTDIR}/usr/local
        DOWNLOAD_DIR     ${DEP_DOWNLOAD_DIR}/Libcxx
        LIST_SEPARATOR   | # Use the alternate list separator because by default all ; are replaced by space
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX:STRING=${DESTDIR}/usr/local
            -DCMAKE_MODULE_PATH:STRING=${PROJECT_SOURCE_DIR}/../cmake/modules
            -DCMAKE_PREFIX_PATH:STRING=${DESTDIR}/usr/local
            -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
            -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE}
            -DCMAKE_BUILD_TYPE=Release
            -DLLVM_ENABLE_PROJECTS=libcxx|libcxxabi
            -DLLVM_USE_SANITIZER=MemoryWithOrigins
        SOURCE_SUBDIR llvm
        BUILD_COMMAND ${CMAKE_COMMAND} --build . --config Release --target cxx cxxabi -- ${_build_j}
        INSTALL_COMMAND ${CMAKE_COMMAND} --build . --config Release --target install-cxx install-cxxabi
            COMMAND     ln -sfn "../local/lib/libc++.so" "${DESTDIR}/usr/lib/libc++.so"
            COMMAND     ln -sfn "../local/lib/libc++.so.1" "${DESTDIR}/usr/lib/libc++.so.1"
            COMMAND     ln -sfn "../local/lib/libc++.so.1.0" "${DESTDIR}/usr/lib/libc++.so.1.0"
            COMMAND     ln -sfn "../local/lib/libc++abi.so" "${DESTDIR}/usr/lib/libc++abi.so"
            COMMAND     ln -sfn "../local/lib/libc++abi.so.1" "${DESTDIR}/usr/lib/libc++abi.so.1"
            COMMAND     ln -sfn "../local/lib/libc++abi.so.1.0" "${DESTDIR}/usr/lib/libc++abi.so.1.0"
)