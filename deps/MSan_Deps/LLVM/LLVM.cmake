set(projectname LLVM)

set(_llvm_cmake_c_flags             "-fPIC")
set(_llvm_cmake_cxx_flags           "-nostdinc++ -isystem ${DESTDIR}/usr/local/include -isystem ${DESTDIR}/usr/local/include/c++/v1 -fPIC")
set(_llvm_cmake_exe_linker_flags    "-stdlib=libc++ -Wl,--rpath=${DESTDIR}/usr/local/lib -L${DESTDIR}/usr/local/lib -fsanitize=memory -fPIC")
set(_llvm_cmake_shared_linker_flags "-stdlib=libc++ -Wl,--rpath=${DESTDIR}/usr/local/lib -L${DESTDIR}/usr/local/lib -fsanitize=memory -fPIC")
set(_llvm_cmake_module_linker_flags "-stdlib=libc++ -Wl,--rpath=${DESTDIR}/usr/local/lib -L${DESTDIR}/usr/local/lib -fsanitize=memory -fPIC")

# TODO: Build LLVM statically and link with Mesa 3D.

ExternalProject_Add(dep_LLVM
        URL https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.1/llvm-project-12.0.1.src.tar.xz
        URL_HASH SHA256=129cb25cd13677aad951ce5c2deb0fe4afc1e9d98950f53b51bdcfb5a73afa0e
        DEPENDS dep_Libcxx
        EXCLUDE_FROM_ALL ON
        INSTALL_DIR      ${DESTDIR}/usr/local
        DOWNLOAD_DIR     ${DEP_DOWNLOAD_DIR}/LLVM
        LIST_SEPARATOR   | # Use the alternate list separator because by default all ; are replaced by space
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX:STRING=${DESTDIR}/usr/local
            -DCMAKE_MODULE_PATH:STRING=${PROJECT_SOURCE_DIR}/../cmake/modules
            -DCMAKE_PREFIX_PATH:STRING=${DESTDIR}/usr/local
            -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
            -DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE}

        -DCMAKE_BUILD_TYPE=Release
        -DLLVM_USE_SANITIZER=MemoryWithOrigins
        #        -DLLVM_ENABLE_PROJECTS=clang|lld|llvm-config
        -DLLVM_ENABLE_PROJECTS=LLVM|llvm-config
        -DLLVM_ENABLE_LIBCXX=ON

         # This forces to make shared library.
        -DLLVM_BUILD_LLVM_DYLIB=ON
        -DLLVM_LINK_LLVM_DYLIB=ON
        -DLLVM_DYLIB_COMPONENTS=all

        #        -DLLVM_TARGETS_TO_BUILD=X86
        -DLLVM_ENABLE_PIC=ON


        -DCMAKE_C_FLAGS:STRING=${_llvm_cmake_c_flags}
        -DCMAKE_CXX_FLAGS:STRING=${_llvm_cmake_cxx_flags}
        -DCMAKE_EXE_LINKER_FLAGS:STRING=${_llvm_cmake_exe_linker_flags}
        -DCMAKE_SHARED_LINKER_FLAGS:STRING=${_llvm_cmake_shared_linker_flags}
        -DCMAKE_MODULE_LINKER_FLAGS:STRING=${_llvm_cmake_module_linker_flags}

        SOURCE_SUBDIR llvm
        BUILD_COMMAND ${CMAKE_COMMAND} --build . --config Release --target LLVM llvm-config -- ${_build_j}
        INSTALL_COMMAND ${CMAKE_COMMAND} --build . --config Release --target install-LLVM install-llvm-config install-llvm-headers
            COMMAND ln -sfn "../local/lib/libLLVM.so" "${DESTDIR}/usr/lib/libLLVM.so"
            COMMAND ln -sfn "../local/lib/libLLVM-12.so" "${DESTDIR}/usr/lib/libLLVM-12.so"
            COMMAND ln -sfn "../local/lib/libLLVM-12.0.1.so" "${DESTDIR}/usr/lib/libLLVM-12.0.1.so"
        )
