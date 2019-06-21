#! /usr/bin/python

import sys
import csv
import subprocess

# TODO: find a csv parsing library and import it

#get this from command-line argument
vampire_executable = "./"+sys.argv[1].strip()

print("Testing "+vampire_executable+"...")

config="testing/test_config.csv"

# Open config in read mode
# Iterate over lines in file (for loop)
# For each line
# 1. Get the problem path as variable path
# 2. Get the options as variable options
# 3. Run (lookup how to run binaries from Python) ./vampire options path --output_mode szs
# 4. Get the return code and check it is correct
# 5. Get the output and check it is correct (need to look in what Vampire prints)

with open(config,"r") as file:
    reader = csv.reader(file)
    headers = reader.next()
    for row in reader:
        data = dict(zip(headers,row))
        name = data['Test Name']
        print('Running test: '+name)
        path = data['Problem Path'] 
        options = data['Option String']
        options_args = options.split(' ')
        if len(options)==0:
            options_args = []
        args = [vampire_executable,path,'--output_mode','szs','-p','off']+options_args
        print('args are '+str(args))
        p = subprocess.Popen(args,stdout = subprocess.PIPE)

        p.wait()
        print(p.returncode)

        passing=False
        if p.returncode ==0:
            output = p.stdout.read()
            expected=data['Expected Status'] 
            for line in output.splitlines():
                if 'SZS' in line and expected in line:
                    passing=True

        if passing:
            print "PASS"
        else:
            print "FAIL"
            sys.exit(-1)

