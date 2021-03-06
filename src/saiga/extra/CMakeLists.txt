#set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(MODULE_NAME "saiga_extra")

add_subdirectory(opencv)


if(GPHOTO2_FOUND)
  add_subdirectory(gphoto2)
endif()

FILE(GLOB EXTRA_SRC  "*.cpp" "*.cu" "*.cc")
FILE(GLOB EXTRA_HEADER  "*.h" "*.hpp" "*.inl")

# the internal files are added to core
SET(SAIGA_ALL_FILES
  ${SAIGA_ALL_FILES}
  ${EXTRA_SRC}
  ${EXTRA_HEADER}
  )

return()

if(SAIGA_BUILD_SHARED)
  add_definitions(-DSAIGA_DLL_EXPORTS)
  add_library(${MODULE_NAME} SHARED ${SAIGA_ALL_FILES})
else ()
  add_library(${MODULE_NAME} STATIC ${SAIGA_ALL_FILES})
endif()

target_link_libraries(${MODULE_NAME} ${EXTRA_TARGETS} ${EXTRA_LIBS})

target_include_directories(
  ${MODULE_NAME}
  PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/>
  "$<BUILD_INTERFACE:${EXTRA_INCLUDES}>"
  #PRIVATE
  # for the internal files
  #${PROJECT_SOURCE_DIR}/src/
  )

install(
  TARGETS ${MODULE_NAME}
  EXPORT ${LIB_EXPORT}
  RUNTIME DESTINATION ${INSTALL_BIN}
  LIBRARY DESTINATION ${INSTALL_SHARED}
  ARCHIVE DESTINATION ${INSTALL_LIB}
  )

#install(
#    EXPORT ${LIB_EXPORT}
#    FILE SaigaTargets.cmake
#    DESTINATION ${INSTALL_SHARE}
#    )
