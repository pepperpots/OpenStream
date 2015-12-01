#!/usr/bin/env python

import sys

output_file = sys.argv[1]

with open(output_file) as f:
    lines = map(lambda x: x.strip(), f.readlines())
    sum = 1023*(1023+1)/2

    write_line = "Producer: write "+str(sum)+" to stream."

    if not write_line in lines:
            sys.stderr.write("Could not find line '"+write_line+"' in output.\n")
            sys.exit(1)
