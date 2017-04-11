#!/bin/bash

# Convert file encoding type from dos 2 unix
find . -regex '.*\.\(cpp\|h\|c\)' -exec dos2unix {} \;

# Remove trailing spaces from source files
find . -regex '.*\.\(cpp\|h\|c\)' -exec sed -i 's/[ \t]*$//g' {} \;

# Replace tabs with 4-spaces in all source files
find . -regex '.*\.\(cpp\|h\|c\)' -exec sed -i 's/\t/    /g' {} \;

# indent code
find . -regex '.*\.\(cpp\|h\|c\)' -exec astyle --indent-switches --indent-cases --pad-oper --pad-comma --add-brackets --indent=spaces=4 -A2 --mode=c  {} \;
