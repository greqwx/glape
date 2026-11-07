# Glape
[greqwx.github.io/glape](https://greqwx.github.io/glape)

Glape is a small scripting language written in C99.<br>
It builds into a single binary with no runtime dependencies.

```glape
load str

define greet(name):
  return "Hello, $name!"

define main():
  loop name in ["Ada", "Bob"]:
    print(str.upper(greet(name)))
```

Save the file as `hello.glape` and run `glape run hello.glape`. If the file contains only
`load` and `define` statements, `main` is called automatically.

## Get Glape
Releases include:
- Linux (amd64, arm64)
- macOS (arm64, amd64)
- Windows (amd64, x86, arm64)

[github.com/greqwx/glape/releases](https://github.com/greqwx/glape/releases).

## Build

You need CMake 3.16 or newer and a C99 compiler.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The interpreter is `build/glape` (`build\glape.exe` on Windows). Visual Studio generators
put it in `build/Release` instead; pass `--config Release` there.

To install:

```
cmake --install build --prefix dist
```

This places the binary in `dist/bin`.


## Tests

`ctest` runs the lexer, AST, script and error tests. Every script test also has a stress
variant with `GLAPE_GC_STRESS=1`, which collects at every safe point and catches values that
were not rooted. Two environment variables help when working on the runtime:

- `GLAPE_GC_STRESS=1` collects on every safe point.
- `GLAPE_GC_STATS=1` prints collections, live objects and live bytes to stderr at the end.

## Project layout

```
cli/      argument parsing and the run/ast/tokens commands
parse/    lexer, parser and AST
runtime/  interpreter, environments, error reporting
mem/      values, arena, garbage collector
libs/     core builtins and loadable modules
tests/    lexer, AST, script and error tests
docs/     documentation site
```

## License
Licensed under MIT License.<br>
Copyright (c) Glape Contributors
