cmake_minimum_required(VERSION 3.5)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang)
project(libeot VERSION 0.01)


# Include directories
include_directories(${CMAKE_SOURCE_DIR}/inc)
 
# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()
 
# Common flags
set(COMMON_FLAGS "-std=c99 -DDECOMPRESS_ON -DCOMPRESS_ON -DLZCOMP_MAIN")
 
# Debug and Release flags
set(DEBUG_FLAGS "-g3 -O0 ${COMMON_FLAGS}")
set(RELEASE_FLAGS "-O2 ${COMMON_FLAGS}")
 
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS ${DEBUG_FLAGS})
else()
    set(CMAKE_C_FLAGS ${RELEASE_FLAGS})
endif()
 
# Library sources
set(LIBEOT_SOURCES
    src/libeot.c
    src/EOT.c
    src/writeFontFile.c
    src/flags.h
    src/triplet_encodings.c
    src/triplet_encodings.h
    src/writeFontFile.h
    src/ctf/parseCTF.c
    src/ctf/parseCTF.h
    src/ctf/parseTTF.c
    src/ctf/parseTTF.h
    src/ctf/SFNTContainer.c
    src/ctf/SFNTContainer.h
    src/util/logging.h
    src/util/max.h
    src/util/stream.h
    src/util/stream.c
    src/lzcomp/ahuff.c
    src/lzcomp/AHUFF.H
    src/lzcomp/bitio.c
    src/lzcomp/BITIO.H
    src/lzcomp/ERRCODES.H
    src/lzcomp/liblzcomp.c
    src/lzcomp/liblzcomp.h
    src/lzcomp/lzcomp.c
    src/lzcomp/LZCOMP.H
    src/lzcomp/mtxmem.c
    src/lzcomp/MTXMEM.H
)
 
# Header files for installation
set(LIBEOT_HEADERS
    inc/libeot/EOT.h
    inc/libeot/EOTError.h
    inc/libeot/libeot.h
)
 
# Add the library
add_library(libeot SHARED ${LIBEOT_SOURCES})
target_include_directories(libeot PUBLIC ${CMAKE_SOURCE_DIR}/inc)
set_target_properties(libeot PROPERTIES PUBLIC_HEADER "${LIBEOT_HEADERS}")
 
# Add the executable
add_executable(eot2ttf src/eot2ttf.c)
target_link_libraries(eot2ttf libeot)

add_executable(lzcomp src/lzcomp/liblzcomp.c)
target_link_libraries(lzcomp libeot)

add_executable(teststream src/tests/test-stream.c src/util/stream.h src/util/stream.c)

# -------------------------------------------------------------------
# Fuzzer source
set(FUZZER_SOURCE src/tests/fuzz-test.cc)
 
# Create the fuzzer executable
add_executable(fuzz_eot2ttf ${FUZZER_SOURCE})

# Link with libeot and required libraries
target_include_directories(fuzz_eot2ttf PRIVATE ${CMAKE_SOURCE_DIR}/inc)
target_link_libraries(fuzz_eot2ttf PRIVATE libeot)

# Enable libFuzzer specific flags
target_compile_options(fuzz_eot2ttf PRIVATE -fsanitize=fuzzer,address,undefined)
target_link_options(fuzz_eot2ttf PRIVATE -fsanitize=fuzzer,address,undefined)

# -------------------------------------------------------------------

enable_testing()
add_test(
    NAME stream-test
    COMMAND $<TARGET_FILE:teststream>
)

# Install targets
install(TARGETS libeot
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/libeot
)
 
install(TARGETS eot2ttf
    RUNTIME DESTINATION bin
)
 
# Generate pkg-config file
configure_file(
    ${CMAKE_SOURCE_DIR}/libeot.pc.in
    ${CMAKE_BINARY_DIR}/libeot.pc
    @ONLY
)
install(FILES ${CMAKE_BINARY_DIR}/libeot.pc DESTINATION lib/pkgconfig)
 
# Extra files
install(FILES LICENSE PATENTS DESTINATION share/libeot)
 
# Clean up
add_custom_target(distclean COMMAND ${CMAKE_COMMAND} -E remove *.pc)
