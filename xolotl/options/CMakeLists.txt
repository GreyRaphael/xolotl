set(XOLOTL_OPTIONS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(XOLOTL_OPTIONS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(XOLOTL_OPTIONS_HEADER_DIR ${XOLOTL_OPTIONS_INCLUDE_DIR}/xolotl/options)

set(XOLOTL_OPTIONS_HEADERS
    ${XOLOTL_OPTIONS_HEADER_DIR}/IOptions.h
    ${XOLOTL_OPTIONS_HEADER_DIR}/Options.h
)

set(XOLOTL_OPTIONS_SOURCES
    ${XOLOTL_OPTIONS_SOURCE_DIR}/Options.cpp
)

add_library(xolotlOptions SHARED
    ${XOLOTL_OPTIONS_SOURCES}
    ${XOLOTL_OPTIONS_HEADERS}
)
target_link_libraries(xolotlOptions PUBLIC
    xolotlUtil
    Boost::program_options
    MPI::MPI_CXX
)
target_include_directories(xolotlOptions PUBLIC
    $<BUILD_INTERFACE:${XOLOTL_OPTIONS_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(xolotlOptions PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib"
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

install(TARGETS xolotlOptions EXPORT Xolotl LIBRARY DESTINATION lib)
