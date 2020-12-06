#!/usr/bin/python3

import os
import sys
import argparse
import shutil
import traceback


def main():
    try:
        shutil.copy('./build/FIPY/release/sigfox/sigfox.a',  './sigfox/modsigfox_FIPY.a')
        shutil.copy('./build/LOPY4/release/sigfox/sigfox.a', './sigfox/modsigfox_LOPY4.a')
        shutil.copy('./build/SIPY/release/sigfox/sigfox.a',  './sigfox/modsigfox_SIPY.a')
    except:
        print("Couldn't copy Sigfox libs!")
        traceback.print_exc()
        return

    print("Sigfox libs copied successfully!")


if __name__ == "__main__":
    main()
