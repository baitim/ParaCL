import os
import glob
import subprocess

curr_dir = os.path.dirname(os.path.realpath(__file__))

def run(answer_dir, exe_file):
    os.system("mkdir -p " + answer_dir)
    file_name = answer_dir + "/answer_" + f'{test_num+1:03}' + ".ans"
    os.system("touch " + file_name)
    os.system("echo -n > " + file_name)
    ans_file = open(file_name, 'w')
    command = exe_file + " < " + file
    ans_file.write(subprocess.check_output(command, shell=True).decode("utf-8"))
    ans_file.close()

paracl_exe = "./paracl"
paracl_answers_dir = curr_dir + "/answers_get"

test_num = 0
files = list(map(str, glob.glob(curr_dir + "/tests_in/test_*.in")))
files.sort()

for file in files :
    run(paracl_answers_dir, paracl_exe)
    test_num += 1
    print("test",  test_num, "passed")