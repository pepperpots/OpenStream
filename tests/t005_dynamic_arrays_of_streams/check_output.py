#!/usr/bin/env python

import sys

output_file = sys.argv[1]

with open(output_file) as f:
    lines = map(lambda x: x.strip(), f.readlines())

    nstreams = 12
    nperm = 10

    permutation1 = range(nperm)

    for i in range(nperm):
        permutation1[i] = (i+7) % nperm

    for idx in range(nperm):
        write_line = "Producer called for idx = "+str(idx)+", i = "+str(permutation1.index(idx))+", 3*i = "+str(3*permutation1.index(idx))
        read_line = "Consumer called for idx = "+str(idx)+", i = "+str(permutation1.index(idx))+", 3*i = "+str(3*permutation1.index(idx))

        if not write_line in lines:
            sys.stderr.write("Could not find line '"+write_line+"' in output.\n")
            sys.exit(1)

        if not read_line in lines:
            sys.stderr.write("Could not find line '"+read_line+"' in output.\n")
            sys.exit(1)

        if lines.index(read_line) < lines.index(write_line):
            sys.stderr.write("Read before write for task "+str(i)+".\n")
            sys.exit(1)
