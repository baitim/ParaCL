<h1 align="center">ParaCL</h1>

## Description

 Implementation of the simple programming language, with flex and bison.<br>
 #### [Documentation](https://github.com/baitim/ParaCL/tree/main/docs/ParaCL.pdf)

## How to integrate

1. Add remote <br>
    <code>conan remote add conan_packages http://77.233.223.124:9300</code>
2. Install requires
    <code>conan install --requires=paracl/1.0@baitim -r=conan_packages</code>

## How to run

1. Clone <br>
    <code>git clone https://github.com/baitim/ParaCL.git</code>

2. Go to folder <br>
    <code>cd ParaCL</code>

3. Prepare conan <br>
    <code>uv sync --group dev; source .venv/bin/activate</code><br>
    <code>conan profile detect --force</code>

4. Init dependencies <br>
    <code>conan install . --build=missing -s build_type=Release</code>

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
        count = 3;
        arr = array(10, 20, 30);

        add_five = func(x) { x + 5; }
        square   = func(x) { x * x; }
        half_    = func(x) : half { x / 2; }

        funcs_size = 3;
        funcs = array(add_five, square, half_);

        print arr;
        print add_five;
        print square;
        print half_;

        num_func = 0;
        while (num_func < funcs_size) {
            i = 0;
            while (i < count) {
                print funcs[num_func](arr[i]);
                i = i + 1;
            }
            num_func = num_func + 1;
        }
    ```
    Output:
    ```
        [10, 20, 30]
        function #default_function_name_001_#
        function #default_function_name_002_#
        function half
        15
        25
        35
        100
        400
        900
        5
        10
        15
    ```

2) Factorial
    ```
        factorial = func(n) : fact {
            if (n <= 1) {
                return 1;
            }
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