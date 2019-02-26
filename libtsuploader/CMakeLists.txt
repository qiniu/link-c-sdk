# ------------------------------------------------------------------------------
# link-c-sdk/libtsuploader CMakeLists.txt
# ------------------------------------------------------------------------------

set(CURRENT_TARGET "tsuploader")

# ------------------------------------------------------------------------------
# Build target
# ------------------------------------------------------------------------------

include_directories(${TOP_DIRS})
include_directories(${CONFIG_DIRS})
include_directories(${DEPENDS_DIRS})
include_directories(${LIBGHTTP_INCLUDE_DIRS})
if(WITH_MQTT)
  include_directories(${LIBTMQTT_INCLUDE_DIRS})
endif(WITH_MQTT)
if(WITH_WOLFSSL)
  include_directories(${WOLFSSL_INCLUDE_DIRS})
endif(WITH_WOLFSSL)

aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SRCS_${CURRENT_TARGET})

aux_source_directory(${DEPENDS_DIRS}/log SRCS_${CURRENT_TARGET})
aux_source_directory(${DEPENDS_DIRS}/flag SRCS_${CURRENT_TARGET})
aux_source_directory(${DEPENDS_DIRS}/b64 SRCS_${CURRENT_TARGET})
aux_source_directory(${DEPENDS_DIRS}/cJSON SRCS_${CURRENT_TARGET})
aux_source_directory(${DEPENDS_DIRS}/hmac_sha1 SRCS_${CURRENT_TARGET})

foreach(f IN LISTS SRCS_${CURRENT_TARGET})
  get_filename_component(b ${f} NAME)
  set_source_files_properties(${f} PROPERTIES
    COMPILE_DEFINITIONS "__FILE_NAME__=\"${b}\"")
endforeach()

# build static target
if(WITH_STATIC)
    add_library(${CURRENT_TARGET}_STATIC STATIC ${SRCS_${CURRENT_TARGET}})
    set_target_properties(${CURRENT_TARGET}_STATIC PROPERTIES OUTPUT_NAME ${CURRENT_TARGET})
    target_compile_definitions(${CURRENT_TARGET}_STATIC PRIVATE "-D__PROJECT__=\"[${CURRENT_TARGET}]\"")
    if(WITH_MQTT)
        add_dependencies(${CURRENT_TARGET}_STATIC mqtt_STATIC)
    endif()
    install(TARGETS ${CURRENT_TARGET}_STATIC
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
endif(WITH_STATIC)

# build shared target
if(WITH_SHARED)
    add_library(${CURRENT_TARGET}_SHARED SHARED ${SRCS_${CURRENT_TARGET}})
    set_target_properties(${CURRENT_TARGET}_SHARED PROPERTIES OUTPUT_NAME ${CURRENT_TARGET})
    set_target_properties(${CURRENT_TARGET}_SHARED PROPERTIES VERSION ${LINK_C_SDK_RELEASE_VERSION} SOVERSION ${LINK_C_SDK_VERSION_MAJOR})
    target_compile_definitions(${CURRENT_TARGET}_SHARED PRIVATE "-D__PROJECT__=\"[${CURRENT_TARGET}]\"")
    if(WITH_MQTT)
        add_dependencies(${CURRENT_TARGET}_SHARED mqtt_SHARED)
    endif()
    install(TARGETS ${CURRENT_TARGET}_SHARED
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
endif(WITH_SHARED)

# ------------------------------------------------------------------------------
# Export head file
# ------------------------------------------------------------------------------

file(GLOB ${CURRENT_TARGET}_HEADERS
  *.h
  )
install(FILES ${${CURRENT_TARGET}_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${CURRENT_TARGET}
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  )

