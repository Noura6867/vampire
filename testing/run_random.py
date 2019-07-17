#! /usr/bin/python

import sys
import csv
import subprocess

#get this from command-line argument
vampire_executable = "./"+sys.argv[1].strip()

print("Raondomly testing "+vampire_executable+"...")

# Run for a minute
time_left = 60

while time_left > 0:

        # TODO: save start time
        start_time = 0

        # TODO: randomly select a problem
        problem = "none" 

        args = [vampire_executable,path,'--output_mode','szs','-p','off','--random_strategy','on','-t','10s',problem]
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

        #TODO: save end time
        end_time = 0
        time_left = time_left - (end_time - start_time)

