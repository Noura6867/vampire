#!/bin/bash

# Compile Vampire
make vampire_dbg -j
make vampire_rel -j

#TODO: check that compilation worked

#TODO: add other static tests

# Dynamic tests
make test_dbg
make test_rel
