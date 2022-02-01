# wordle solver

This project is a solver for all wordle-like game.
`/data` contains all different game dataset
`/src` contains frontend source

# dependency
* spdlog (automatically fetch)
* tbb (automatically fetch)
* sdl (automatically fetch)
* emscripten (optional)

# build
```bash
cmake -DCMAKE_BUILD_TYPE=Release -Bbuild .
cmake --build build
```

# execute
`build/src/wordle_solver` for command line interface
`build/src/wordle_solver_imgui` for graphical user interface
