# Find C++ Lambdas That Capture Vars by Reference

C++ code like this would be dangerous:
```cpp
Foo foo;
auto invoke = [&]() {
    foo.bar();
};
dispatch(invoke);
```

This clang plugin would emit warning on such lamda definitions that implicitly
or explicitly capture `this`, a pointer, or a reference.

## Usage
Compile by `make`, and use `clang.sh` as the C++ compiler; or pass
`$(cmdline.sh)` to clang++ argument.


### Environment Variables

* `CHKREF_IGNORE_POINTER_TYPE`: types whose pointers would not cause warning if
  get captured. Separated by semicoln.


## Example
`make run` would output the following:

```txt
test.cpp:14:22: warning: Lambda captures this: class `C' with fields: m_x m_ptr m_y
            auto g = [=]() {
                     ^
test.cpp:21:22: warning: Lambda captures var by ref: cap
            auto g = [&cap] {
                     ^
test.cpp:28:22: warning: Lambda captures pointer: m (type int *)
            auto g = [m]() {
                     ^
test.cpp:34:22: warning: Lambda captures var by ref: v
            auto g = [&]() {
                     ^
test.cpp:41:22: warning: Lambda captures pointer: p (type int *)
            auto g = [=]() {
                     ^
test.cpp:72:22: warning: Lambda captures pointer: dependent (type int *)
            auto g = [=]() {
                     ^
6 warnings generated.
```
