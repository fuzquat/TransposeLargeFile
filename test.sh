#!/bin/bash
../Release/TransposeStuff.exe i $1 $1.intermediate
sort -n -k1,1 -k2,2 $1.intermediate -o $1.sorted
../Release/TransposeStuff.exe m $1.sorted $1.transposed

../Release/TransposeStuff.exe i $1.transposed $1.intermediate2
sort -n -k1,1 -k2,2 $1.intermediate2 -o $1.sorted2
../Release/TransposeStuff.exe m $1.sorted2 $1.recreated
