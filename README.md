<h1 align="center">ParaCL</h1>

## Description

 Implementation of the simple programming language, with flex and bison.

## How to run

1. Clone <br>
    <code>git clone https://github.com/baitim/ParaCL.git</code>

2. Go to folder <br>
    <code>cd ParaCL</code>

3. Init submodules <br>
    write <code>git submodule update --init --recursive</code> in terminal

4. Build <br>
    <code>cmake . -B build; cmake --build build</code>

5. Run <br>
    <code>./build/src/paracl</code>

## How to test

* Testing
    - End to end <br>
        <code>ctest --test-dir build</code><br>
        maybe you will need these flags for the ctest <code>--rerun-failed --output-on-failure</code>

<p align="center"><img src="https://github.com/baitim/ParaCL/blob/main/images/cat.gif" width="50%"></p>

## Support
**This project is created by [baitim](https://t.me/bai_tim)**