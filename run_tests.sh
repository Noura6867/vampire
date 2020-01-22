#!/bin/bash

# Remove prevoius vampires
rm vampire_*
make clean

# Compile Vampire
make vampire_dbg 
make vampire_rel 

# Check that compilation worked

if test -f vampire_dbg_*; then
	echo "vampire_dbg compile success"
else
	echo "vampire_dbg compile failure"
	exit -1
fi
if test -f vampire_rel_*; then
	echo "vampire_rel compile success"
else
	echo "vampire_rel compile failure"
	exit -1
fi


#TODO: add other static tests

# Dynamic tests
make test_dbg
make test_rel

# Random tests
#python testing/run_random.py
