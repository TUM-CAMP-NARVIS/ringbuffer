# Header
configure_file(${PROJECT_SOURCE_DIR}/include/ringbuffer/config.h.in ${CMAKE_BINARY_DIR}/include/ringbuffer/config.h @ONLY)


set(RINGBUFFER_HEADERS
        "${CMAKE_BINARY_DIR}/include/ringbuffer/config.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/visibility.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/types.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/common.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/detail/debug_trap.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/detail/memory.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/detail/cuda.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/detail/trace.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/detail/affinity.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/detail/ring_realloc_lock.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/detail/ring_state.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/detail/guarantee.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/detail/util.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/detail/signal.h"

        "${PROJECT_SOURCE_DIR}/include/ringbuffer/ring.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/sequence.h"
        "${PROJECT_SOURCE_DIR}/include/ringbuffer/span.h"
        )

set(RINGBUFFER_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/common.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/detail/memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/detail/cuda.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/detail/trace.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/detail/affinity.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/detail/ring_realloc_lock.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/detail/guarantee.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ring.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sequence.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/span.cpp
        )

add_library(ringbuffer ${RINGBUFFER_SOURCES} ${RINGBUFFER_HEADERS})

set_target_properties(ringbuffer PROPERTIES CXX_STANDARD 11)
set_target_properties(ringbuffer PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(ringbuffer PUBLIC
        CONAN_PKG::Boost
        CONAN_PKG::spdlog
        )

target_include_directories(ringbuffer PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/include
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_BINARY_DIR}/include
        ${PROJECT_BINARY_DIR}/src
        ${PROJECT_SOURCE_DIR}/src
        )

if(NOT RINGBUFFER_BUILD_STATIC)
    target_compile_definitions(ringbuffer PRIVATE "RINGBUFFER_EXPORTS")
endif()


if (RINGBUFFER_WITH_CUDA)
    set_target_properties(ringbuffer PROPERTIES CUDA_STANDARD 11)
    target_include_directories(ringbuffer PUBLIC ${CUDA_INCLUDE_DIRS})
    target_link_libraries(ringbuffer PUBLIC ${RINGBUFFER_CUDA_LIBRARIES})
endif()

if (RINGBUFFER_WITH_OMP)
    target_link_libraries(ringbuffer PUBLIC OpenMP::OpenMP_CXX)
endif()

if (RINGBUFFER_WITH_NUMA)
    target_link_libraries(ringbuffer PUBLIC NUMA::NUMA)
endif()


install(TARGETS ringbuffer EXPORT ringbufferConfig
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ringbuffer DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")
install(FILES ${PROJECT_BINARY_DIR}/include/ringbuffer/config.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ringbuffer)

# This makes the project importable from the install directory
# Put config file in per-project dir (name MUST match), can also
# just go into 'cmake'.
install(EXPORT ringbufferConfig DESTINATION share/ringbuffer/cmake)

# This makes the project importable from the build directory
export(TARGETS ringbuffer FILE ringbufferConfig.cmake)

