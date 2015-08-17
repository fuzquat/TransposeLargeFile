#!/bin/bash
./transpose i $1 $1.intermediate
sort -n -k1,1 -k2,2 $1.intermediate -o $1.sorted
./transpose m $1.sorted $1.transposed

./transpose i $1.transposed $1.intermediate2
sort -n -k1,1 -k2,2 $1.intermediate2 -o $1.sorted2
./transpose m $1.sorted2 $1.recreated
diff $1 $1.recreated
