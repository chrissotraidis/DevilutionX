# Same MIT backend revision integrated by upstream DevilutionX PR #8482.
include(functions/FetchContent_MakeAvailableExcludeFromAll)
include(FetchContent)
set(MPQFS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(mpqfs
    GIT_REPOSITORY https://github.com/diasurgical/mpqfs.git
    GIT_TAG 9423bd48d04c8a0c51d3df6ce328e66785e3b441
)
FetchContent_MakeAvailableExcludeFromAll(mpqfs)
