# compile-time-json

This library allows you to create C++ structures with standard layout that hold JSON object from compile time JSON string while allowing
modification of values and reassignment at runtime with constant time access. The JSON objects can also be passed as template parameters
as the objects are literal types. See the example for further details on the interfaces and possible use cases.

## How to Build

This library requires a compiler with C++20 support. You can compile the example using the Bazel build system with the following command:

```sh
bazel run //example:example
```
