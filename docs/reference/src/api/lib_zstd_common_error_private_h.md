# lib/zstd/common/error_private.h

## `static INLINE_KEYWORD UNUSED_ATTR void _force_has_format_string(const char *format, ...) {`


Ignore: this is an internal helper.

This is a helper function to help force C99-correctness during compilation.
Under strict compilation modes, variadic macro arguments can't be empty.
However, variadic function arguments can be. Using a function therefore lets
us statically check that at least one (string) argument was passed,
independent of the compilation flags.


---

## `#define _FORCE_HAS_FORMAT_STRING(...) \ if (0) {`


Ignore: this is an internal helper.

We want to force this function invocation to be syntactically correct, but
we don't want to force runtime evaluation of its arguments.


---

## `#define RETURN_ERROR_IF(cond, err, ...) \ if (cond) {`


Return the specified error if the condition evaluates to true.

In debug modes, prints additional information.
In order to do that (particularly, printing the conditional that failed),
this can't just wrap RETURN_ERROR().


---

## `#define RETURN_ERROR(err, ...) \ do {`


Unconditionally return the specified error.

In debug modes, prints additional information.


---

## `#define FORWARD_IF_ERROR(err, ...) \ do {`


If the provided expression evaluates to an error code, returns that error code.

In debug modes, prints additional information.


---

