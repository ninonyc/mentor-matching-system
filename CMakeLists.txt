cmake_minimum_required(VERSION 3.16)
project(mentor_matching_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(matching_core algorithm/src/matching.cpp algorithm/src/stability.cpp)
target_include_directories(matching_core PUBLIC algorithm/include)

add_executable(matching_demo algorithm/src/demo.cpp)
target_link_libraries(matching_demo PRIVATE matching_core)

add_executable(stability_demo algorithm/src/stability_demo.cpp)
target_link_libraries(stability_demo PRIVATE matching_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(matching_smoke_tests tests/matching_smoke_tests.cpp)
    target_link_libraries(matching_smoke_tests PRIVATE matching_core)
    add_test(NAME matching_smoke_tests COMMAND matching_smoke_tests)
    add_executable(stability_tests tests/stability_tests.cpp)
    target_link_libraries(stability_tests PRIVATE matching_core)
    add_test(NAME stability_tests COMMAND stability_tests)
    add_executable(core_validation tests/core_validation.cpp)
    target_link_libraries(core_validation PRIVATE matching_core)
    add_test(NAME core_validation COMMAND core_validation)
endif()

# Optional: Phases 1–3 still build without OpenSSL.
option(MENTOR_BUILD_API "Build the Phase 5 C++ API" OFF)
if(MENTOR_BUILD_API)
    enable_language(C)
    find_package(OpenSSL 3 REQUIRED)
    find_package(Threads REQUIRED)
    add_library(mentor_sqlite STATIC backend/third_party/sqlite3.c)
    target_compile_definitions(mentor_sqlite PRIVATE SQLITE_THREADSAFE=1)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # Match the direct build's optimization level for upstream SQLite.
        target_compile_options(mentor_sqlite PRIVATE $<$<CONFIG:Release>:-O2>)
    endif()
    target_include_directories(mentor_sqlite PUBLIC backend/third_party)
    add_executable(mentor_api backend/src/database.cpp backend/src/auth.cpp
        backend/src/service.cpp backend/src/server.cpp)
    target_include_directories(mentor_api PRIVATE backend/include)
    target_include_directories(mentor_api SYSTEM PRIVATE backend/third_party)
    target_link_libraries(mentor_api PRIVATE matching_core mentor_sqlite OpenSSL::Crypto
        Threads::Threads ${CMAKE_DL_LIBS})
    if(BUILD_TESTING)
        find_package(Python3 3.8 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            add_test(NAME api_integration COMMAND ${CMAKE_COMMAND} -E env
                MENTOR_API_BINARY=$<TARGET_FILE:mentor_api>
                ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/backend/tests/test_api.py)
            set_tests_properties(api_integration PROPERTIES TIMEOUT 120)
            add_test(NAME database_schema COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/database/tests/test_schema.py)
        endif()
    endif()
endif()
