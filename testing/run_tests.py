#! /usr/bin/python

# TODO: find a csv parsing library and import it

vampire_executable = #get this from command-line arguments

config="testing/test_config.csv"

# Open config in read mode
# Iterate over lines in file (for loop)
# For each line
# 1. Get the problem path as variable path
# 2. Get the options as variable options
# 3. Run (lookup how to run binaries from Python) ./vampire options path --output_mode szs
# 4. Get the return code and check it is correct
# 5. Get the output and check it is correct (need to look in what Vampire prints)
