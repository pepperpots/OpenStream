#!/bin/bash

./bzip2 -i input.txt -s 24 -b 7
mv out.compress.7 out.compress.7.1
./stream_bzip2 -i input.txt -s 24 -b 7
echo "===>testing correctness by comparing zipped file"
difference=`diff out.compress.7*`
if ["$difference" -eq ""]
then
    echo "===>compared success!"
else
    echo "===>failed"
fi