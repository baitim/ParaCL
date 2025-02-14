import os
import glob
import subprocess
from pathlib import Path

class bcolors:
    INFO = "\033[93m"
    FAIL = '\033[31m'
    ENDC = '\033[0m'

tests_dir = str(Path(__file__).parent)
build_dir = str(Path.cwd())
is_OK = True

def run(program, input, exe_file):
    global is_OK
    command = exe_file + " " + program + " < " + input
    result = subprocess.run(command, shell=True, capture_output=True)
    if result.returncode == 0:
        print(bcolors.FAIL + "result is valid: " + program + bcolors.ENDC)
        is_OK = False
    return result.returncode

paracl_exe = build_dir + "/paracl"
program_files = list(map(str, glob.glob(tests_dir + "/tests_error_in/test_*.in")))
program_files.sort()

input_data_files = list(map(str, glob.glob(tests_dir + "/input4tests_in/input_*.in")))
input_data_files.sort()

if (len(input_data_files) != len(program_files)):
    print("count of input files != count of program files")
    exit(1)

for test_num in range(0, len(input_data_files)) :
    run(program_files[test_num], input_data_files[test_num], paracl_exe)
    print("test",  test_num + 1, "processed")

if not(is_OK):
    exit(1)