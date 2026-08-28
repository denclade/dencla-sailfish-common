# TODO: Description


## Tests

```bash
# Standalone-Build + Tests dieses Repos:
cmake -S . -B build -DDENCLA_COMMON_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
```bash
git clone --recurse-submodules https://github.com/dencla/dencla-sailfish-common.git
# Update existing Checkout:
git submodule update --init --recursive
```