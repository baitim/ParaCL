<h1 align="center">ParaCL</h1>

## Description

 Implementation of the simple programming language, with flex and bison.

## How to run

1. Clone <br>
    <code>git clone https://github.com/baitim/ParaCL.git</code>

2. Go to folder <br>
    <code>cd ParaCL</code>

3. Prepare conan <br>
    <code>conan profile detect --force</code>

4. Init dependencies <br>
    <code>conan install . --build=missing -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True -s compiler.cppstd=gnu20</code><br>
    maybe you will need these flags for the conan <code>-s build_type=Debug</code>

5. Build <br>
    <code>cmake . -B build -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake; cmake --build build</code>

6. Run <br>
    <code>./build/src/paracl</code>

<p align="center"><img src="https://github.com/baitim/ParaCL/blob/main/images/cat.gif" width="50%"></p>

## Support
**This project is created by [baitim](https://t.me/bai_tim)**