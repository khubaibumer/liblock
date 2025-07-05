# Set the minimum required version for CMake and define the project.
cmake_minimum_required(VERSION 3.12)
project(DualLockLibrary LANGUAGES C CXX)

# Set the C and C++ standards.
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- C Library (liblock) ---
add_library(liblock src/liblock/lock.c)
target_include_directories(liblock PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/liblock"
)
find_package(Threads REQUIRED)
target_link_libraries(liblock PUBLIC Threads::Threads)
set_target_properties(liblock PROPERTIES OUTPUT_NAME "lock")

# --- C++ Library (liblock++) ---
add_library(liblock++ src/liblockpp/Lock.cpp)
target_include_directories(liblock++ PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/liblockpp"
)
target_link_libraries(liblock++ PUBLIC Threads::Threads)
set_target_properties(liblock++ PROPERTIES OUTPUT_NAME "lock++")

# --- Executable Definitions ---
add_executable(c_benchmark test/c_benchmark.c)
target_link_libraries(c_benchmark PRIVATE liblock)
message(STATUS "Target 'c_benchmark' builds against liblock (Pure C API).")

add_executable(cpp_benchmark test/cpp_benchmark.cpp)
target_link_libraries(cpp_benchmark PRIVATE liblock++)
message(STATUS "Target 'cpp_benchmark' builds against liblock++ (Modern C++ API).")


# --- Sanitizer and Compiler Flags ---
# Define the sanitizer flags in a list for clarity.
#set(TSAN_COMPILE_FLAGS "-fsanitize=thread" "-g" "-fno-omit-frame-pointer")
#set(TSAN_LINK_FLAGS "-fsanitize=thread")
# Define the sanitizer flags in a list for clarity.
set(TSAN_COMPILE_FLAGS "")
set(TSAN_LINK_FLAGS "")

# Apply flags to all targets using the modern, per-target approach.
# This is much safer and more reliable than setting global CMAKE_C_FLAGS.
foreach (target c_benchmark cpp_benchmark liblock liblock++)
    # Add common warning flags
    target_compile_options(${target} PRIVATE -Wall -Wextra -O3 -march=native -Ofast)

    # Add ThreadSanitizer compile flags
    target_compile_options(${target} PRIVATE ${TSAN_COMPILE_FLAGS})

    # Add ThreadSanitizer link flags (crucial for linking the TSan runtime)
    target_link_options(${target} PRIVATE ${TSAN_LINK_FLAGS})
endforeach ()

# Apply optimization for realistic performance numbers on benchmarks.
# Note: Sanitizers often work best with -O1, but we will keep your -O3 for now.
target_compile_options(c_benchmark PRIVATE -O3 -march=native -Ofast)
target_compile_options(cpp_benchmark PRIVATE -O3 -march=native -Ofast -flto)


install(TARGETS liblock
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
)

install(FILES
        include/liblock/lock_c_api.h
        include/lock.h
        include/lock_types.h
        DESTINATION include/liblock
)


install(TARGETS liblock++
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
)

install(FILES
        include/liblockpp/Lock.hpp
        include/liblockpp/ILock.hpp
        include/lock.h
        include/lock_types.h
        DESTINATION include/liblockpp
)

