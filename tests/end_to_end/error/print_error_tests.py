import os
import glob
import subprocess
from pathlib import Path
from PIL import ImageFont, ImageDraw, Image

curr_dir = str(Path(__file__).parent)
proj_dir = curr_dir + "/../../.."

class bcolors:
    INFO = "\033[93m"
    FAIL = "\033[31m"
    ENDC = "\033[0m"

def run(program, input, exe_file):
    command = exe_file + " " + program + " --analyze " + " < " + input
    result = subprocess.run(command, shell=True, capture_output=True)
    return result.stdout.decode("utf-8")

paracl_exe = proj_dir + "/build/src/paracl"
program_files = list(map(str, glob.glob(proj_dir + "/tests/end_to_end/error/tests_error_in/test_*.in")))
program_files.sort()

input_data_files = list(map(str, glob.glob(proj_dir + "/tests/end_to_end/error/input4tests_in/input_*.in")))
input_data_files.sort()

if (len(input_data_files) != len(program_files)):
    print("count of input files != count of program files")
    exit()

result_str = ""
for test_num in range(0, len(input_data_files)) :
    result_str += bcolors.INFO + "test " + str(test_num + 1) + ":" + bcolors.ENDC + "\n"
    result_str += run(program_files[test_num], input_data_files[test_num], paracl_exe)
    result_str += bcolors.INFO + "\"--------------------------------------------------------\"\n" + bcolors.ENDC

print(result_str)

#------------------------------------------------------------------------------------#
# ChatGPT4 is used below, sorry I don't wont to make this hardcode now,
# I only want to make pretty picture
#------------------------------------------------------------------------------------#
# UPD:
# It took me 3 hours, ChatGPT is not capable of anything more than doing homework in English
# I **** it
# I'd rather do it myself
#------------------------------------------------------------------------------------#

def ansi_to_color(ansi_code):
    ansi_colors = {
        '30': (0, 0, 0), '31': (255, 0, 0), '32': (0, 255, 0), '33': (255, 255, 0),
        '34': (0, 0, 255), '35': (255, 0, 255), '36': (0, 255, 255), '37': (255, 255, 255),
        '90': (128, 128, 128), '91': (255, 192, 192), '92': (192, 255, 192), '93': (255, 255, 192),
        '94': (192, 192, 255), '95': (255, 192, 255), '96': (192, 255, 255), '97': (255, 255, 255)
    }
    return ansi_colors.get(ansi_code, (255, 255, 255))

def create_image_from_colored_text(colored_text, file_name):
    font_size = 14
    font = ImageFont.truetype("DejaVuSansMono.ttf", font_size)
    max_width = 800
    line_height = font_size + 2
    img_height = line_height * colored_text.count('\n') + line_height * 2
    img = Image.new('RGB', (max_width, img_height), (0, 0, 0))
    draw = ImageDraw.Draw(img)

    x_offset = 10
    y_offset = 10
    current_color = (255, 255, 255)

    parts = colored_text.split('\n')

    for line in parts:
        i = 0
        while i < len(line):
            if line[i] == '\x1b':
                end_index = line.find('m', i)
                if end_index != -1:
                    ansi_code = line[i + 2:end_index]
                    current_color = ansi_to_color(ansi_code)
                    i = end_index + 1
                    continue

            text_to_draw = line[i]
            draw.text((x_offset, y_offset), text_to_draw, fill=current_color, font=font)

            text_width = draw.textlength(text_to_draw, font=font)

            x_offset += text_width
            
            if x_offset > max_width - 10:
                y_offset += line_height
                x_offset = 10
            
            i += 1

        y_offset += line_height
        x_offset = 10

    img.save(file_name)

create_image_from_colored_text(str(result_str), "images/errors.png")