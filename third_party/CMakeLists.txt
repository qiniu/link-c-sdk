include(ExternalProject)

if (WITH_OPENSSL)
    ExternalProject_Add(external-wolfssl
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wolfssl
            CONFIGURE_COMMAND ./autogen.sh &&
                ./configure
                --enable-static
                --disable-shared
                --disable-debug
                --enable-opensslextra
                --prefix=${CMAKE_CURRENT_SOURCE_DIR}/wolfssl/output/${HOST}
            BUILD_COMMAND make
            BUILD_IN_SOURCE 1
            INSTALL_COMMAND make install
    )
endif()
