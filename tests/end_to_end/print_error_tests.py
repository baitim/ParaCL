import os
import glob
import subprocess

class bcolors:
    INFO = "\033[93m"
    FAIL = '\033[31m'
    ENDC = '\033[0m'

curr_dir = os.path.dirname(os.path.realpath(__file__))
os.system('cd ' + curr_dir)

def run(program, exe_file):
    global is_OK
    command = [exe_file, program, "--analyze"]
    result = subprocess.run(command, capture_output=True)
    print(result.stdout.decode('utf-8'), end="")

paracl_exe = "./build/src/paracl"
program_files = list(map(str, glob.glob("tests/end_to_end/tests_error_in/test_*.in")))
program_files.sort()

test_num = 1
for program in program_files :
    print(bcolors.INFO + "test " + str(test_num) + ":" + bcolors.ENDC)
    run(program, paracl_exe)
    test_num += 1
    print("--------------------------------------------------------")

os.system('cd -')