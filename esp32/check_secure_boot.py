import argparse


def main():
    cmd_parser = argparse.ArgumentParser()
    cmd_parser.add_argument('--SECURE', default=None)
    cmd_args = cmd_parser.parse_args()

    secure = cmd_args.SECURE
    with open("sdkconfig.h") as sdkconfig:
        if any("CONFIG_SECURE_BOOT_ENABLED" in l for l in sdkconfig.readlines()):
            if(secure != "on"):
                print("If CONFIG_SECURE_BOOT_ENABLED is defined in sdkconfig.h, the SECURE=on must be used when building the Firmware!")
                # Non zero exit code means error
                exit(1)

if __name__ == "__main__":
    main()