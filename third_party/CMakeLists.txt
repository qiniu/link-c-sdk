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
    ExternalProject_Add(external-curl
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/curl
            CONFIGURE_COMMAND ./buildconf &&
                ./configure
                --prefix=${CMAKE_CURRENT_SOURCE_DIR}/curl/output/${HOST}
                --enable-shared=no
                --without-zlib
                --without-libidn2
                --disable-libtool-lock
                --enable-http
                --disable-ftp
                --disable-file
                --disable-ldap
                --disable-ldaps
                --disable-rtsp
                --disable-proxy
                --disable-dict
                --disable-telnet
                --disable-tftp
                --disable-pop3
                --disable-imap
                --disable-smb
                --disable-smtp
                --disable-gopher
                --disable-manual
                --disable-libcurl-option
                --enable-ipv6
                --disable-largefile
                --disable-sspi
                --disable-ntlm-wb
                --disable-unix-sockets
                --disable-cookies
                --disable-crypto-auth
                --disable-tls-srp
                --with-wolfssl=${CMAKE_CURRENT_SOURCE_DIR}/wolfssl/output/${HOST}
                --without-ssl
                --host=${HOST}
            BUILD_COMMAND make
            BUILD_IN_SOURCE 1
            INSTALL_COMMAND make install
    )
else()
    ExternalProject_Add(external-curl
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/curl
            CONFIGURE_COMMAND ./buildconf &&
                ./configure
                --prefix=${CMAKE_CURRENT_SOURCE_DIR}/curl/output/${HOST}
                --enable-shared=no
                --without-zlib
                --without-libidn2
                --disable-libtool-lock
                --enable-http
                --disable-ftp
                --disable-file
                --disable-ldap
                --disable-ldaps
                --disable-rtsp
                --disable-proxy
                --disable-dict
                --disable-telnet
                --disable-tftp
                --disable-pop3
                --disable-imap
                --disable-smb
                --disable-smtp
                --disable-gopher
                --disable-manual
                --disable-libcurl-option
                --enable-ipv6
                --disable-largefile
                --disable-sspi
                --disable-ntlm-wb
                --disable-unix-sockets
                --disable-cookies
                --disable-crypto-auth
                --disable-tls-srp
                --without-ssl
                --host=${HOST}
            BUILD_COMMAND make
            BUILD_IN_SOURCE 1
            INSTALL_COMMAND make install
    )
endif()
