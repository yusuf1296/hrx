if("unit" IN_LIST ENABLE_TESTS OR "functional" IN_LIST ENABLE_TESTS)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)

  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 063de7e9578f82b369302001269680b4b1553359 # v1.18.0
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(googletest)

  include(GoogleTest)
endif()

if("unit" IN_LIST ENABLE_TESTS)
  add_subdirectory(UnitTests)
endif()

if("functional" IN_LIST ENABLE_TESTS)
  add_subdirectory(FunctionalTests)
endif()
