<h1 align="center">ParaCL</h1>

## Description

 Implementation of the simple programming language, with flex and bison.

## How to integrate
 
 use [storage](https://github.com/baitim/ConanPackages), project = "paracl", version = "1.0", user = "baitim"

## How to run

1. Clone <br>
    <code>git clone https://github.com/baitim/ParaCL.git</code>

2. Go to folder <br>
    <code>cd ParaCL</code>

3. Prepare conan <br>
    <code>uv sync --group dev; source .venv/bin/activate</code><br>
    <code>conan profile detect --force</code>

4. Init dependencies <br>
    <code>conan install . --build=missing -s compiler.cppstd=gnu20</code><br>
    maybe you will need these flags for the conan <code>-s build_type=Debug</code>

5. Build <br>
    <code>cmake --preset release; cmake --build build/Release</code>

6. Run <br>
    <code>./build/Release/src/paracl \<program\></code>

## How to test

* Testing
    - valid & error end to end <br>
        <code>ctest --test-dir build/Release --output-on-failure</code>

* Print
    - error end to end <br>
        <code>python3 tests/end_to_end/error/print_error_tests.py</code>


## Errors
#### [Examples](https://github.com/baitim/ParaCL/tree/main/images/errors.png)

* ### Compile errors
    * Unknown token
    * Undeclared variable
    * Wrong types

## Supported flags
* print info <code>--help</code>
* analization without execution <code>--analyze_only</code>

<p align="center"><img src="https://github.com/baitim/ParaCL/blob/main/images/cat.gif" width="50%"></p>

## Support
**This project is created by [baitim](https://t.me/bai_tim)**