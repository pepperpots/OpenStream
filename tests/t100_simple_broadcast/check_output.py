#!/usr/bin/env python

import sys

output_file = sys.argv[1]

with open(output_file) as f:
    lines = map(lambda x: x.strip(), f.readlines())

    write_line = "Producer: write 12345 to stream."

    if not write_line in lines:
            sys.stderr.write("Could not find line '"+write_line+"' in output.\n")
            sys.exit(1)

    for i in range(10):
        read_line = "Task "+str(i)+": read 12345 from stream."

        if not read_line in lines:
            sys.stderr.write("Could not find line '"+read_line+"' in output.\n")
            sys.exit(1)

        if lines.index(read_line) < lines.index(write_line):
            sys.stderr.write("Read before write for task "+str(i)+".\n")
            sys.exit(1)
