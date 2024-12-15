import os
import glob
import subprocess
from pathlib import Path

curr_dir = str(Path(__file__).parent)
proj_dir = curr_dir + "/../../.."

def run(program, input, answer_dir, exe_file):
    os.system("mkdir -p " + answer_dir)
    file_name = answer_dir + "/answer_" + f'{test_num+1:03}' + ".ans"
    os.system("touch " + file_name)
    os.system("echo -n > " + file_name)
    ans_file = open(file_name, 'w')
    command = exe_file + " " + program + " < " + input
    ans_file.write(subprocess.check_output(command, shell=True).decode("utf-8"))
    ans_file.close()

paracl_exe = proj_dir + "/build/src/paracl"
paracl_answers_dir =  proj_dir + "/tests/end_to_end/valid/answers_get"

program_files = list(map(str, glob.glob(proj_dir + "/tests/end_to_end/valid/tests_valid_in/test_*.in")))
program_files.sort()

input_data_files = list(map(str, glob.glob(proj_dir + "/tests/end_to_end/valid/input4tests_in/input_*.in")))
input_data_files.sort()

if (len(input_data_files) != len(program_files)):
    print("count of input files != count of program files")
    exit()

for test_num in range(0, len(input_data_files)) :
    run(program_files[test_num], input_data_files[test_num], paracl_answers_dir, paracl_exe)
    print("test",  test_num + 1, "processed")