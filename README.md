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
