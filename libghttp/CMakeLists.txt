# ------------------------------------------------------------------------------
# link-c-sdk/libghttp CMakeLists.txt
# ------------------------------------------------------------------------------

set(CURRENT_TARGET "ghttp")

# ------------------------------------------------------------------------------
# Build target
# ------------------------------------------------------------------------------

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CONFIG_DIRS})
include_directories(${DEPENDS_DIRS})
include_directories(${LIBGHTTP_INCLUDE_DIRS})

if(WITH_WOLFSSL)
  include_directories(${WOLFSSL_INCLUDE_DIRS})
endif(WITH_WOLFSSL)

aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SRCS_${CURRENT_TARGET})

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
    if(WITH_WOLFSSL)
        add_dependencies(${CURRENT_TARGET}_STATIC external-wolfSSL)
    endif()
    install(TARGETS ${CURRENT_TARGET}_STATIC
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
endif(WITH_STATIC)

# build shared target
if(WITH_SHARED)
    add_library(${CURRENT_TARGET}_SHARED SHARED ${SRCS_${CURRENT_TARGET}})
    set_target_properties(${CURRENT_TARGET}_SHARED PROPERTIES OUTPUT_NAME ${CURRENT_TARGET})
    set_target_properties(${CURRENT_TARGET}_SHARED PROPERTIES VERSION ${LINK_C_SDK_RELEASE_VERSION} SOVERSION ${LINK_C_SDK_VERSION_MAJOR})
    target_compile_definitions(${CURRENT_TARGET}_SHARED PRIVATE "-D__PROJECT__=\"[${CURRENT_TARGET}]\"")
    if(WITH_WOLFSSL)
        add_dependencies(${CURRENT_TARGET}_SHARED external-wolfSSL)
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
  ghttp.h
  ghttp_constants.h
  )
install(FILES ${${CURRENT_TARGET}_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${CURRENT_TARGET}
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  )

#############################################################################

