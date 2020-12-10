
set(TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_memory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_ring.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_fibers.cpp
        )

add_executable(ringbuffer_test ${TEST_SOURCES})

target_link_libraries(ringbuffer_test PUBLIC 
        ringbuffer
        ${RINGBUFFER_CUDA_LIBRARIES}
        )

# conan weirdness ...
if (TARGET CONAN_PKG::GTest)
  target_link_libraries(ringbuffer_test PUBLIC CONAN_PKG::GTest)
else()
  target_link_libraries(ringbuffer_test PUBLIC CONAN_PKG::gtest)
endif()

if(ENABLE_FIBERS)
    target_link_libraries(ringbuffer_test PUBLIC
            CONAN_PKG::fiberpool
            )
    set_target_properties(ringbuffer_test PROPERTIES CXX_STANDARD 17)
endif()

target_include_directories(ringbuffer_test
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/include
        ${PROJECT_BINARY_DIR}/src
        ${PROJECT_SOURCE_DIR}/src
        )
