<h1 align="center">ParaCL</h1>

## Description

 Implementation of the simple programming language, with flex and bison.

## How to run

1. Clone <br>
    <code>git clone https://github.com/baitim/ParaCL.git</code>

2. Go to folder <br>
    <code>cd ParaCL</code>

3. Prepare conan <br>
    write <code>conan profile detect --force</code> in terminal

4. Init dependencies <br>
    write <code>conan install . --build=missing -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True -s compiler.cppstd=gnu20</code> in terminal <br>
    maybe you will need these flags for the conan <code>-s build_type=Debug</code>
    
5. Build <br>
    <code>cmake . -B build -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake; cmake --build build</code>

6. Run <br>
    <code>./build/src/paracl [--analyze]? \<program\></code>

## How to test

* Testing
    - valid & error end to end <br>
        <code>ctest --test-dir build --output-on-failure</code>

* Print
    - error end to end <br>
        <code>python3 tests/end_to_end/print_error_tests.py</code>


## Features

* Ability to check whether values are defined <br>
    include flag <code>--analyze</code>

## Compile errors
* Unknown token
* Undeclared variable
* Wrong types
[examples](https://github.com/baitim/ParaCL/tree/main/images/errors.png)

## Runtime errors

* Check that loop/fork condition is undef

<p align="center"><img src="https://github.com/baitim/ParaCL/blob/main/images/cat.gif" width="50%"></p>

## Support
**This project is created by [baitim](https://t.me/bai_tim)**