#!/usr/bin/env python

import sys

output_file = sys.argv[1]

with open(output_file) as f:
    lines = map(lambda x: x.strip(), f.readlines())

    for i in range(10):
        write_line = "Producer sends: "+str(i)+" on x[0] and ("+str(2*i)+", "+str(3*i)+") on x[1]"
        read_line = "Consumer receives: "+str(i)+" on x[0] and ("+str(2*i)+", "+str(3*i)+") on x[1]"

        if not write_line in lines:
            sys.stderr.write("Could not find line '"+write_line+"' in output.\n")
            sys.exit(1)

        if not read_line in lines:
            sys.stderr.write("Could not find line '"+read_line+"' in output.\n")
            sys.exit(1)

        if lines.index(read_line) < lines.index(write_line):
            sys.stderr.write("Read before write for task "+str(i)+".\n")
            sys.exit(1)
