import os
import glob
import subprocess
from pathlib import Path

class bcolors:
    INFO = "\033[93m"
    FAIL = '\033[31m'
    ENDC = '\033[0m'

tests_dir = str(Path(__file__).parent)
proj_dir  = str(Path.cwd())
is_OK = True

def run(program, input, exe_file):
    global is_OK
    with open(input, 'r') as infile:
        result = subprocess.run([exe_file, program], stdin=infile, capture_output=True)
    if result.returncode == 0:
        print(bcolors.FAIL + "result is valid: " + program + bcolors.ENDC)
        is_OK = False
    return result.returncode

paracl_exe = proj_dir + "/../../src/paracl"
program_files = list(map(str, glob.glob(tests_dir + "/tests_in/test_*.in")))
program_files.sort()

input_data_files = list(map(str, glob.glob(tests_dir + "/input4tests_in/input_*.in")))
input_data_files.sort()

if (len(input_data_files) != len(program_files)):
    print("count of input files != count of program files")
    exit(1)

for test_num in range(0, len(input_data_files)) :
    run(program_files[test_num], input_data_files[test_num], paracl_exe)

if not(is_OK):
    exit(1)