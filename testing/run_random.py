#! /usr/bin/python

import sys
import csv
import subprocess
from datetime import datetime
import random
import fnmatch
import os

#get this from command-line argument
vampire_executable = "./"+sys.argv[1].strip()

print("Raondomly testing "+vampire_executable+"...")

# Load problems
problems = list()
for root, dirnames, filenames in os.walk('TPTP/Problems'):
    for filename in fnmatch.filter(filenames,'*.p'):
        name = os.path.join(root,filename)
        problems.append(name)

# Run for a minute
time_left = 60

while time_left > 0:

        # save start time
        start_time = datetime.now() 

        # randomly select a problem
        problem = random.choice(problems) 
        print("Using problem: "+problem)

        # expected
        expected = "Unknown"
        with open(problem,"r") as prob_file:
            for line in prob_file:
                if "Status" in line:
                    expected = line.split()[3]
                    print("expected is "+expected)
                    break

        # pick a time to run randomly between 1 and 15 seconds
        # mean time will be 7.5 seconds, so in 60 seconds can run at leats 8 tests
        time = random.randint(1,16)

        args = [vampire_executable,'--include','TPTP','--output_mode','szs','-p','off','--random_strategy','on','-t',str(time)+'s',problem]
        #print('args are '+str(args))
        p = subprocess.Popen(args,stdout = subprocess.PIPE)

        p.wait()

        output = p.stdout.read()
        output_lines = output.splitlines()
        print("Strategy used is "+output_lines[0])
        print("Return code is "+str(p.returncode))

        passing=False
        timed_out=False
        incomplete=False
        user_error=False
        for line in output_lines:
                if 'SZS status' in line:
                    if expected != "Unknown":
                        passing = (expected in line)
                    else:
                        passing=True
                if 'Time limit' in line:
                    timed_out = True
                if 'User error' in line:
                    user_error=True
                if 'Refutation not found,' in line:
                    incomplete=True

        if passing:
            print "PASS"
        elif timed_out:
            print "TIMED OUT"
        elif incomplete:
            print "INCOMPLETE STRATEGY"
        elif user_error:
            print "STRATEGY INVALID"
        else:
            print "FAIL"
            for line in output_lines:
                print(line)
            sys.exit(-1)

        # save end time
        end_time = datetime.now() 
        time_left = time_left - (end_time - start_time).total_seconds()

