<h1 align="center">ParaCL</h1>

## Description

 Implementation of the simple programming language, with flex and bison.

## How to run

1. Clone <br>
    write <code>git clone https://github.com/baitim/ParaCL.git</code> in terminal

2. Go to folder <br>
    write <code>cd ParaCL</code> in terminal

3. Prepare conan <br>
    write <code>conan profile detect --force</code> in terminal

4. Init dependencies <br>
    write <code>conan install . --build=missing -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True -s compiler.cppstd=gnu20</code> in terminal <br>
    maybe you will need these flags for the conan <code>-s build_type=Debug</code>

5. Build <br>
    write <code>cmake . -B build -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake; cmake --build build</code> in terminal

6. Run <br>
    write <code>./build/src/paracl</code> in terminal <br>

<p align="center"><img src="https://github.com/baitim/ParaCL/blob/main/images/cat.gif" width="50%"></p>

## Support
<img src="https://github.com/baitim/ParaCL/blob/main/images/orangutan.gif" width="8%" align="left">

**This project is created by [baitim](https://t.me/bai_tim)**