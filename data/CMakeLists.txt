if (DeveloperBuild)
    set(INSERT_DEV_ENV_PATHS_HERE
"       export PATH=\"${CMAKE_BINARY_DIR}/src/build_shell:$PATH\"
        export PATH=\"${CMAKE_BINARY_DIR}/src/jsonmod:$PATH\"
")
else()
    message(STATUS FOOODWQODPWQOFKWE)
    install(DIRECTORY build_shell/scripts
            DESTINATION share/build_shell
            USE_SOURCE_PERMISSIONS)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/build_shell/build_shell_completion.bash
            DESTINATION share/build_shell
            PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif(DeveloperBuild)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/build_shell/build_shell_completion.bash
  COMMAND ${CMAKE_COMMAND} -P
    ${CMAKE_CURRENT_BINARY_DIR}/build_shell/configure_build_shell_completion.cmake
  DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/build_shell/configure_build_shell_completion.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/build_shell/build_shell_completion.bash.template
  COMMENT "Adding paths if in DeveloperBuild"
  VERBATIM)

add_custom_target(configure_build_shell_completion ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/build_shell/build_shell_completion.bash)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build_shell/configure_build_shell_completion.cmake.template
               ${CMAKE_CURRENT_BINARY_DIR}/build_shell/configure_build_shell_completion.cmake
               @ONLY)
