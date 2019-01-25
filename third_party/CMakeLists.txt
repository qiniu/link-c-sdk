include(ExternalProject)

if (WITH_OPENSSL OR WITH_WOLFSSL)
    ExternalProject_Add(external-wolfssl
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wolfssl
            CONFIGURE_COMMAND ./autogen.sh &&
                ./configure
                --enable-small-stack
                --prefix=${CMAKE_CURRENT_SOURCE_DIR}/wolfssl/output/${HOST}
                --host=${HOST}
                --enable-static
                --disable-shared
                --disable-debug
            BUILD_COMMAND make
            BUILD_IN_SOURCE 1
            INSTALL_COMMAND make install
    )
    if (WITH_MQTT)
        ExternalProject_Add(external-wolfMQTT
                DEPENDS external-wolfssl
                SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wolfMQTT
                CONFIGURE_COMMAND ./autogen.sh &&
                    ./configure
                    --prefix=${CMAKE_CURRENT_SOURCE_DIR}/wolfMQTT/output/${HOST}
                    --with-lib-prefix=${CMAKE_CURRENT_SOURCE_DIR}/wolfssl/output/${HOST}
                    --host=${HOST}
                    --enable-static
                    --disable-shared
                    --disable-examples
                    --disable-debug
                BUILD_COMMAND make
                BUILD_IN_SOURCE 1
                INSTALL_COMMAND make install
        )
    endif()
endif()
