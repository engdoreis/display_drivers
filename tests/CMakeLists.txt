
# Fetch GoogleTest library. 
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/release-1.12.1.zip
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(FTB
  GIT_REPOSITORY    https://github.com/nothings/stb
  GIT_TAG           f0569113c93ad095470c54bf34a17b36646bbbb5
)
FetchContent_Populate(FTB)

set(TEST_NAME ${NAME}_test)

# add_compile_options(-O0 -g3)
add_executable(${TEST_NAME} main.cc )

target_include_directories(${TEST_NAME} PRIVATE "../" "${ftb_SOURCE_DIR}")
target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main ${NAME} )

add_test(NAME Test_0 COMMAND ${TEST_NAME})

