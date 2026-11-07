# Glape

Glape is a small scripting language with an interpreter written in C.

```
load str

define greet(name):
  return "Hello, $name!"

define main():
  loop n in ["Ada", "Bob"]:
    print(str.upper(greet(n)))
```
