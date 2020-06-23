import argparse
from subprocess import Popen, PIPE


def main():
    cmd_parser = argparse.ArgumentParser()
    cmd_parser.add_argument('--VERSION', default=None)
    cmd_args = cmd_parser.parse_args()
    version = cmd_args.VERSION

    process = Popen(["xtensa-esp32-elf-gcc", "--version", "."], stdout=PIPE)
    (output, err) = process.communicate()
    exit_code = process.wait()
    if(exit_code == 0):
        if version in output:
            print("Xtensa version OK!")
            exit(0)
        else:
            print("xtensa-esp32-elf-gcc version must be: " + version)
            # Non zero exit code means error
            exit(1) 
    else:
        print("xtensa-esp32-elf-gcc dropped an error, its version cannot be checked!")
        # Non zero exit code means error
        exit(1) 


if __name__ == "__main__":
    main()