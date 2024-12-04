import os
import glob
import subprocess

class bcolors:
    FAIL = '\033[31m'
    ENDC = '\033[0m'


curr_dir = os.path.dirname(os.path.realpath(__file__))
is_OK = True

def run(program, exe_file):
    global is_OK
    command = [exe_file, program, "--analyze"]
    result = subprocess.run(command, capture_output=True)
    if result.returncode == 0:
        print(bcolors.FAIL + "result is valid: " + program + bcolors.ENDC)
        is_OK = False
    return result.returncode

paracl_exe = "./paracl"
program_files = list(map(str, glob.glob(curr_dir + "/tests_error_in/test_*.in")))
program_files.sort()

test_num = 0
for program in program_files :
    run(program, paracl_exe)
    test_num += 1
    print("test",  test_num, "processed")

if not(is_OK):
    exit(1)