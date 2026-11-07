# Errors

## Catching an error

Put `catch` after a call to handle an error from it. The block runs only if the call fails.

```
define divide(a, b):
  return a / b

define main():
  divide(1, 0) catch err:
    print("failed: $err")    // failed: division by zero
  print("still running")
```

## Raising an error

```
define check_age(age):
  if age < 0:
    fail("age cannot be negative")
  return age
```

`fail` accepts any value, not only strings.

## Uncaught errors

The program stops, prints where the error happened and exits with code 1.

```
file.glape:2:12: division by zero
  at inner (file.glape:2:12)
  at main (file.glape:8:9)
```
