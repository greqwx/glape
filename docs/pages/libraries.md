# Libraries

## Built in

These work without `load`.

| Function | Description |
|---|---|
| `print(x)` | print a value |
| `len(x)` | length of a string, list or map |
| `type(x)` | name of the type, like `"int"` or `"list"` |
| `to_str(x)` | convert to a string |
| `to_int(x)` | convert to an int |
| `to_float(x)` | convert to a float |
| `fail(x)` | raise an error |

## Modules

Add `load` at the top of the file to use them.

| Module | Functions |
|---|---|
| `str` | upper, lower, trim, trim_start, trim_end, find, contains, starts_with, ends_with, split, join, replace, repeat, substr, slice |
| `list` | push, pop, insert, remove, contains, index_of, reverse, slice, extend, clear |
| `map` | keys, values, has, get, set, remove, clear, update |
| `math` | abs, sqrt, pow, exp, log, log10, sin, cos, tan, asin, acos, atan, atan2, floor, ceil, round, min, max, clamp |
| `os` | hostname, username, home_dir, temp_dir, cpu_count, and the values name, arch, sep, eol |

## os

`os.name` is `"linux"`, `"macos"` or `"windows"`. `os.arch` is `"amd64"`, `"arm64"`, `"x86"` or `"arm"`.
`os.sep` is the path separator (`/` or `\`) and `os.eol` is the line ending (`\n` or `\r\n`).

The functions take no arguments. `os.hostname()`, `os.username()`, `os.home_dir()` and
`os.temp_dir()` return strings and raise an error when the system cannot provide the value.
`os.cpu_count()` returns the number of logical processors as an int, at least 1.

```
load os

print(os.name)          // linux
print(os.cpu_count())   // 8
print(os.temp_dir())    // /tmp
```

## Example

```
load str, list, map

words = str.split("the quick brown fox", " ")
list.push(words, "jumps")
print(str.join(words, "|"))     // the|quick|brown|fox|jumps

counts = {}
loop w in words:
  if map.has(counts, w):
    map.set(counts, w, map.get(counts, w) + 1)
  else:
    map.set(counts, w, 1)
```
