# Want to transpose a large (gigabytes) TSV or other delimited file?

Super simple problem.  I have a file with:

```
1a 1b 1c
2a 2b 2c
```
But I would really rather have:
```
1a 2a
1b 2b
1c 2c
```

Which is a really simple problem.  Until the input file has millions of rows and columns and ends up being multiple gigabytes in size.  Then things get annoying.

This thing has a few goals:
* Try and be reasonably fast.
* Try not to reinvent the wheel.
* Try and be somewhat portable.

So the approach it takes is: 

Read the file, write out each row and column with where we would really perfer to have it.

```
0 0 1a
1 0 1b
2 0 1c
0 1 2a
1 1 2b
2 1 2c
```
use the standard unix sort (which works well on *any* sized file) to sort it

```
sort -n -k1,1 -k2,2 in_file -o out_file
```

```
0 0 1a
0 1 2a
1 0 1b
1 1 2b
2 0 1c
2 1 2c
```

Now notice its in the right order.  We read it in and then write it out with newlines in the right places:

```
1a 2a
1b 2b
1c 2c
```

So what we have done here is take the transpose my file problem and broken it down to:
* Sequentally read the file, writing out a list of new rows and columns.
* Tell sort to sort the list we created.
* Sequentally read the sorted file, writing out the transposed file.

This is awesome because each step is simple to understand and implement. Additionally it requires very little RAM, and alot of disk.  But if you are working with giant files you have lots of disk right?

### How to build it.

./build.sh invokes g++4.6 and creates transpose

it requires g++ 4.9 (C++ 14 FTW)
which you can get on ubuntu with:

```shell
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install g++-4.9
```

### How to use it
Run test.sh on a file. It does the following:
```shell
#!/bin/bash
# Write the list to be sorted
./transpose i $1 $1.intermediate
# Sort the list
sort -n -k1,1 -k2,2 $1.intermediate -o $1.sorted
# Create the transposed file from the sorted list
./transpose m $1.sorted $1.transposed

#Do the above steps again on the transposed file:

./transpose i $1.transposed $1.intermediate2
sort -n -k1,1 -k2,2 $1.intermediate2 -o $1.sorted2
./transpose m $1.sorted2 $1.recreated

# Diff the transpose of the transpose -- it should be identical to what you started with.
diff $1 $1.recreated
```

###Words of warning

This is setup to work on tab delimited files that end with a newline.  

It wants files that look like:
```
<val>\t<val>\t<val>\n
<val>\t<val>\t<val>\n
```
where \n is a newline and \t is a tab.


