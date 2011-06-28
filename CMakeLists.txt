cmake_minimum_required(VERSION 2.6)

project(tagger)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)
find_package(Taglib REQUIRED)

include_directories(${TAGLIB_INCLUDES})

add_executable(tagger tagger.cpp)
set_target_properties(tagger PROPERTIES OUTPUT_NAME tagger)
target_link_libraries(tagger ${TAGLIB_LIBRARIES})
install(TARGETS tagger DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

