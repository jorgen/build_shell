include_directories(${PROJECT_SOURCE_DIR}/src/3rdparty/optionparser/src)
include_directories(${PROJECT_SOURCE_DIR}/src/3rdparty/json_tools/src)

file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB JSONTOOLS_FILES ${PROJECT_SOURCE_DIR}/src/3rdparty/json_tools/src/*.cpp)

add_executable(buildset_convert ${SRC_FILES} ${JSONTOOLS_FILES})

if (NOT DeveloperBuild)
    install(TARGETS DESTINATION bin)
endif (NOT DeveloperBuild)
