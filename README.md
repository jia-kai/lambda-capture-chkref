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

## Example
`make run` would output the following:

```txt
test.cpp:7:22: warning: Lambda captures this
            auto g = [=]() {
                     ^
test.cpp:14:22: warning: Lambda captures var by ref: cap
            auto g = [&cap] {
                     ^
test.cpp:21:22: warning: Lambda captures pointer: m (type int *)
            auto g = [m]() {
                     ^
test.cpp:27:22: warning: Lambda captures var by ref: v
            auto g = [&]() {
                     ^
4 warnings generated.
```
