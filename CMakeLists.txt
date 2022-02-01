# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2022 Shen-Ta Hsieh

cmake_minimum_required(VERSION 3.22)
project(wordle_solver)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(FetchContent)

option(WORDLE_CLI "build cli version" ON)
option(WORDLE_IMGUI "build imgui version" ON)

add_subdirectory(src)
