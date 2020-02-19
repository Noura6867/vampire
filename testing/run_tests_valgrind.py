#! /usr/bin/python

import sys
import csv
import subprocess

# TODO: find a csv parsing library and import

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
        args = ['valgrind',vampire_executable,path,'--output_mode','szs','-p','off']+options_args
        print('args are '+str(args))
        p = subprocess.Popen(args, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
        
        std_out, std_err = p.communicate()
        
        p.wait()
        print(p.returncode)

        output = std_out + std_err

        passing=False
        valgrind_okay=False
        if p.returncode == 0:
            expected=data['Expected Status'] 
            for line in output.splitlines():
                if 'SZS status' in line and expected in line:
                    passing=True
                if 'ERROR SUMMARY: 0 errors' in line:
                    valgrind_okay=True
        else:
            print "FAIL (return code)"
            sys.exit(-1)
        
        if passing: 
            if valgrind_okay:
                print "PASS"
            else:
                print "FAIL (valgrind)"
                sys.exit(-1)
        else:
            print "FAIL (status)"
            sys.exit(-1)

