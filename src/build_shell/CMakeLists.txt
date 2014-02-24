find_package (Threads)

include_directories(${PROJECT_SOURCE_DIR}/src/3rdparty/optionparser/src)
include_directories(${PROJECT_SOURCE_DIR}/src/3rdparty/json_tools/src)

file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB JSONTOOLS_FILES ${PROJECT_SOURCE_DIR}/src/3rdparty/json_tools/src/*.cpp)
add_executable(build_shell ${SRC_FILES} ${JSONTOOLS_FILES})
target_link_libraries(build_shell ${CMAKE_THREAD_LIBS_INIT} )

if (DeveloperBuild)
    add_definitions(-DSCRIPTS_PATH="${PROJECT_SOURCE_DIR}/data/build_shell/scripts")
else()
    add_definitions(-DSCRIPTS_PATH="${CMAKE_INSTALL_PREFIX}/share/build_shell/scripts")
    install(TARGETS build_shell DESTINATION bin)
endif(DeveloperBuild)

