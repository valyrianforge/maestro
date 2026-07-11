# maestro_worktree: per-worker git worktree isolation. Qt-free. WorktreeManager
# is platform-neutral (all git access via IGitRunner); the popen-based
# RealGitRunner is POSIX-only, mirroring PosixProcessBackend.
set(MAESTRO_WORKTREE_SOURCES src/WorktreeManager.cpp)

if(NOT WIN32)
  list(APPEND MAESTRO_WORKTREE_SOURCES src/RealGitRunner.cpp)
endif()

add_library(maestro_worktree ${MAESTRO_WORKTREE_SOURCES})
add_library(maestro::worktree ALIAS maestro_worktree)

target_include_directories(maestro_worktree PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_features(maestro_worktree PUBLIC cxx_std_20)
maestro_set_warnings(maestro_worktree)
