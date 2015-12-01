#!/usr/bin/env python

import sys

output_file = sys.argv[1]

with open(output_file) as f:
    lines = map(lambda x: x.strip(), f.readlines())

    write_line = "Producer 1: write 42 43 to stream."
    if not write_line in lines:
        sys.stderr.write("Could not find line '"+write_line+"' in output.\n")

    read_line = "Consumer 2: read 42 43 from stream."
    if not read_line in lines:
        sys.stderr.write("Could not find line '"+write_line+"' in output.\n")

    if lines.index(read_line) < lines.index(write_line):
        sys.stderr.write("Read before write for task "+str(i)+".\n")
        sys.exit(1)

    for i in range(9):
        write_line = "Producer 2: write "+str(2*i)+" "+str(2*i+1)+" to stream."
        read_line = "Consumer "+str(1 + i % 2)+": read "+str(2*i)+" "+str(2*i+1)+" from stream."

        if not write_line in lines:
            sys.stderr.write("Could not find line '"+write_line+"' in output.\n")
            sys.exit(1)

        if not read_line in lines:
            sys.stderr.write("Could not find line '"+read_line+"' in output.\n")
            sys.exit(1)

        if lines.index(read_line) < lines.index(write_line):
            sys.stderr.write("Read before write for task "+str(i)+".\n")
            sys.exit(1)

    write_line = "Producer 2: write 18 19 to stream."
    if not write_line in lines:
        sys.stderr.write("Could not find line '"+write_line+"' in output.\n")
