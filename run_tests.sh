#!/bin/bash

# Compile Vampire
make vampire_dbg 
make vampire_rel 

#TODO: check that compilation worked

#TODO: add other static tests

# Dynamic tests
make test_dbg
make test_rel
