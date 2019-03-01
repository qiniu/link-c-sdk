# ------------------------------------------------------------------------------
# link-c-sdk/third_party CMakeLists.txt
# ------------------------------------------------------------------------------

include(ExternalProject)

if(WITH_STATIC)
  set(THIRDPARTY_WITH_STATIC "yes")
else()
  set(THIRDPARTY_WITH_STATIC "no")
endif()

if(WITH_SHARED)
  set(THIRDPARTY_WITH_SHARED "yes")
else()
  set(THIRDPARTY_WITH_SHARED "no")
endif()

if(WITH_WOLFSSL)
    set(WOLFMQTT_WITH_TLS "yes")
    set(WOLFMQTT_DEPENDS "external-wolfSSL")
else()
    set(WOLFMQTT_WITH_TLS "no")
    set(WOLFMQTT_DEPENDS "")
endif()


if(WITH_WOLFSSL)
  ExternalProject_Add(external-wolfSSL
    EXCLUDE_FROM_ALL TRUE
    BUILD_IN_SOURCE TRUE
    URL https://github.com/wolfSSL/wolfssl/archive/v3.15.7-stable.tar.gz
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ./autogen.sh &&
      ./configure
        CC=${CMAKE_C_COMPILER}
        CXX=${CMAKE_CXX_COMPILER}
        --enable-static=${THIRDPARTY_WITH_STATIC}
        --enable-shared=${THIRDPARTY_WITH_SHARED}
        --disable-debug
        --host=${C_COMPILER_MACHINE}
        --prefix=${WOLFSSL_PREFIX}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    TEST_COMMAND ""
    )
  install(DIRECTORY ${WOLFSSL_PREFIX}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
  install(DIRECTORY ${WOLFSSL_PREFIX}/lib/
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

if(WITH_MQTT)
  ExternalProject_Add(external-wolfMQTT
    DEPENDS ${WOLFMQTT_DEPENDS}
    EXCLUDE_FROM_ALL TRUE
    BUILD_IN_SOURCE TRUE
    URL https://github.com/wolfSSL/wolfMQTT/archive/v1.2.tar.gz
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ./autogen.sh &&
      ./configure
        CC=${CMAKE_C_COMPILER}
        CXX=${CMAKE_CXX_COMPILER}
        --enable-tls=${WOLFMQTT_WITH_TLS}
        --with-lib-prefix=${WOLFSSL_PREFIX}
        --prefix=${WOLFMQTT_PREFIX}
        --host=${C_COMPILER_MACHINE}
        --enable-static=${THIRDPARTY_WITH_STATIC}
        --enable-shared=${THIRDPARTY_WITH_SHARED}
        --disable-examples
        --disable-debug
    BUILD_COMMAND make
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND make install
    TEST_COMMAND ""
    )
    
  install(DIRECTORY ${WOLFMQTT_PREFIX}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
  install(DIRECTORY ${WOLFMQTT_PREFIX}/lib/
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif(WITH_MQTT)
