<h1 align="center">ParaCL</h1>

## Description

 Implementation of the simple programming language, with flex and bison.<br>
 #### [Documentation](https://github.com/baitim/ParaCL/tree/main/docs/ParaCL.pdf)

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
    <code>conan install . --build=missing</code><br>
    maybe you will need these flags for the conan <code>-s build_type=Debug</code>

5. Build <br>
    <code>cmake --preset release; cmake --build build/Release</code>

6. Run <br>
    <code>./build/Release/src/paracl \<program\></code>

## Supported flags
* print info <code>--help</code>
* analization without execution <code>--analyze_only</code>

## How to test

* Testing
    - valid & error end to end <br>
        <code>ctest --test-dir build/Release --output-on-failure</code>

* Print
    - error end to end <br>
        <code>python3 tests/end_to_end/error/print_error_tests.py</code>

## Example of errors
#### [Examples](https://github.com/baitim/ParaCL/tree/main/images/errors.png)

## Example of usage
ParaCL example programs
1) Array of functions
    ```
        count = 5;
        arr = array(5, 3, 8, 1, 7);

        sum = func(a, n) {
            s = 0;
            i = 0;
            while (i < n) {
                s = s + a[i];
                i = i + 1;
            }
            s;
        }

        max = func(a, n) {
            m = a[0];
            i = 1;
            while (i < n) {
                if (a[i] > m) {
                    m = a[i];
                }
                i = i + 1;
            }
            m;
        }

        funcs = array(sum, max);

        print arr;

        s = funcs[0](arr, count);
        print s;

        m = funcs[1](arr, count);
        print m;
    ```
    Output:
    ```
        [5, 3, 8, 1, 7]
        function #default_function_name_001_#
        24
        function #default_function_name_002_#
        8
    ```

2) Factorial
    ```
        factorial = func(n) : fact {
            if (n <= 1) {
                return 1;
            };
            return n * fact(n - 1);
        }

        print fact(5);
        print factorial;
        print factorial(7);
    ```
    Output:
    ```
        120
        function fact
        5040
    ```

<p align="center"><img src="https://github.com/baitim/ParaCL/blob/main/images/cat.gif" width="50%"></p>

## Support
**This project is created by [baitim](https://t.me/bai_tim)**