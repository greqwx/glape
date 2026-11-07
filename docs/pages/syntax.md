# Syntax

## Hello world

```
define main():
  print("Hello, world!")
```

Save it as `hello.glape` and run `glape run hello.glape`. `main` is called automatically.

Blocks are written with a colon and an indented body. Comments start with `//`.

## Variables

Assign a value and the variable exists. There is nothing to declare.

```
name = "Ada"
age = 36
pi = 3.14
ready = true
nothing = nil
```

## Numbers

Whole numbers are `int`, numbers with a dot or an exponent are `float`. You can use `_` to make long numbers readable.

```
print(2 + 3 * 4)    // 14
print(7 / 2)        // 3
print(7.0 / 2)      // 3.5
print(7 % 3)        // 1
print(1_000_000)    // 1000000
```

Dividing two ints gives an int. Use a float on either side to get a decimal result.

## Strings

Put `$name` or `${expression}` inside a string to insert a value. Use `\$` for a literal dollar sign.

```
name = "Ada"
print("Hi, $name")              // Hi, Ada
print("2 + 2 = ${2 + 2}")       // 2 + 2 = 4
print("line one\nline two")
print("a" + "b")                // ab
```

## Lists

```
nums = [10, 20, 30]
print(nums[0])      // 10
nums[1] = 25
print(len(nums))    // 3
print(nums)         // [10, 25, 30]
```

Indexes start at 0. Going past the end is an error.

## Maps

```
user = {name: "Ada", age: 36}
print(user.name)        // Ada
print(user["age"])      // 36
user["city"] = "London"
```

Keys are strings. `{name: "Ada"}` and `{"name": "Ada"}` are the same.

## Conditions

```
if age >= 18:
  print("adult")
else:
  print("minor")
```

Comparisons are `== != < <= > >=`. Combine them with `and`, `or` and `not`.

`nil`, `false`, `0`, `""`, `[]` and `{}` count as false. Everything else counts as true.

## Loops

Loop over a list, or over the keys of a map.

```
loop n in [1, 2, 3, 4, 5]:
  if n == 2:
    skip          // go to the next item
  if n == 5:
    break         // stop the loop
  print(n)
```

This prints 1, 3 and 4. `continue` works the same as `skip`.

## Functions

```
define add(a, b):
  return a + b

print(add(2, 3))    // 5
```

A function without `return` gives back `nil`. Calling a function with the wrong number of arguments is an error.

## Modules

```
load str, math

print(str.upper("hello"))    // HELLO
print(math.max(3, 8))        // 8
```

See [Libraries](libraries) for everything that is available.
