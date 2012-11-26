#!/bin/bash

./bzip2 input.txt 24
mv out.compress.7 out.compress.7.1
./stream_bzip2 input.txt 24
echo "===>testing correctness by comparing zipped file"
difference=`diff out.compress.7*`
if ["$difference" -eq ""]
then
    echo "===>compared success!"
else
    echo "===>failed"
fi