#pragma once

/* NOTE(jordan): compile-time string check method shamelessly stolen from:
 * https://stackoverflow.com/questions/5721813/compile-time-assert-for-string-equality
 */
constexpr bool static_string_eq (char const *a, char const *b) {
  return (*a && *b)
    ? (*a == *b && static_string_eq(a + 1, b + 1))
    : (!*a && !*b);
}
