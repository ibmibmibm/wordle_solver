# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2022 Shen-Ta Hsieh

if(EMSCRIPTEN)
  set(CMAKE_SYSTEM_PROCESSOR WebAssembly32)
  set(CMAKE_OSX_ARCHITECTURES arm64)
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  add_compile_options(
    -mmutable-globals
    -mnontrapping-fptoint
    -msign-ext
    -mreference-types
    -mbulk-memory
    -msimd128
    -matomics
    -pthread
  )
  add_definitions(
    -D__TBB_RESUMABLE_TASKS_USE_THREADS
    -D__TBB_WEAK_SYMBOLS_PRESENT=0
    -D__TBB_DYNAMIC_LOAD_ENABLED=0
  )
  file(GLOB DATA_FILES ../data/*/*.txt)
endif()

find_package(TBB QUIET)
if(NOT TBB_FOUND)
  FetchContent_Declare(
    TBB
    GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
    GIT_TAG        v2021.5.0
  )
  set(TBB_TEST OFF CACHE BOOL "Enable testing" FORCE)
  set(TBBMALLOC_BUILD OFF CACHE BOOL "Enable tbbmalloc build" FORCE)
  set(BUILD_SHARED_LIBS OFF)
  FetchContent_MakeAvailable(TBB)
endif()

if(WORDLE_CLI)
  find_package(spdlog 1.9.2 QUIET)
  if(NOT spdlog_FOUND)
    FetchContent_Declare(
      spdlog
      GIT_REPOSITORY https://github.com/gabime/spdlog.git
      GIT_TAG        v1.9.2
    )
    FetchContent_MakeAvailable(spdlog)
  endif()

  add_executable(wordle_solver
    cli.cpp
  )

  target_compile_features(wordle_solver
    PUBLIC
    cxx_std_20
  )

  target_compile_options(wordle_solver
    PRIVATE
    -Wall
  )

  target_include_directories(wordle_solver
    PUBLIC
    ../include
  )

  target_link_libraries(wordle_solver
    PUBLIC
    spdlog::spdlog_header_only
    TBB::tbb
  )

  if(EMSCRIPTEN)
    target_compile_options(wordle_solver
      PUBLIC
      "SHELL:-s USE_SDL=0"
      "SHELL:-s DISABLE_EXCEPTION_CATCHING=1"
    )
    target_link_options(wordle_solver
      PUBLIC
      -pthread
      "SHELL:-s USE_SDL=0"
      "SHELL:-s DISABLE_EXCEPTION_CATCHING=1"
      "SHELL:-s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency"
      "SHELL:-s INITIAL_MEMORY=67108864"
      "SHELL:-s WASM=1"
      "SHELL:-s NO_EXIT_RUNTIME=0"
      "SHELL:-s ASSERTIONS=1"
      --no-heap-copy
      "SHELL:--preload-file ../data@/data"
    )
    set_property(TARGET wordle_solver APPEND PROPERTY LINK_DEPENDS ${DATA_FILES})
  endif()
endif()

if(WORDLE_IMGUI)
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.86
  )
  FetchContent_MakeAvailable(imgui)
  add_library(imgui
    OBJECT
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  )
  target_include_directories(imgui
    PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
  )
  if(NOT EMSCRIPTEN)
    find_package(SDL2)
    if(NOT SDL_FOUND)
      FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.0.20
      )
      set(SDL_SHARED_ENABLED_BY_DEFAULT OFF CACHE BOOL "Build a shared version of the library" FORCE)
      FetchContent_MakeAvailable(SDL2)
    endif()
    target_link_libraries(imgui
      PUBLIC
      SDL2::SDL2-static
      GL
    )
  else()
    target_compile_options(imgui
      PUBLIC
      "SHELL:-s USE_SDL=2"
      "SHELL:-s DISABLE_EXCEPTION_CATCHING=1"
    )
    target_link_options(imgui
      PUBLIC
      -pthread
      "SHELL:-s USE_SDL=2"
      "SHELL:-s DISABLE_EXCEPTION_CATCHING=1"
      "SHELL:-s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency"
      "SHELL:-s INITIAL_MEMORY=67108864"
      "SHELL:-s WASM=1"
      "SHELL:-s NO_EXIT_RUNTIME=0"
      "SHELL:-s ASSERTIONS=1"
      "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell_minimal.html"
      --no-heap-copy
      "SHELL:--preload-file ../data@/data"
      "SHELL:--preload-file ../fonts@/fonts"
    )
    set_property(TARGET imgui APPEND PROPERTY LINK_DEPENDS ${DATA_FILES})
  endif()

  add_executable(wordle_solver_imgui
    imgui.cpp
  )

  target_compile_features(wordle_solver_imgui
    PUBLIC
    cxx_std_20
  )

  target_compile_definitions(wordle_solver_imgui
    PUBLIC
    -DTBB_PREVIEW_TASK_GROUP_EXTENSIONS=1
  )

  target_compile_options(wordle_solver_imgui
    PRIVATE
    -Wall
  )

  target_include_directories(wordle_solver_imgui
    PUBLIC
    ../include
  )

  target_link_libraries(wordle_solver_imgui
    PUBLIC
    TBB::tbb
    imgui
  )
endif()
