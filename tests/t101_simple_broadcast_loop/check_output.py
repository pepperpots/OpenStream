#!/usr/bin/env python

import sys

output_file = sys.argv[1]

with open(output_file) as f:
    lines = map(lambda x: x.strip(), f.readlines())

    for j in range(10):
        write_line = "Producer "+str(j)+": write "+str(j)+" to stream."

        if not write_line in lines:
                sys.stderr.write("Could not find line '"+write_line+"' in output.\n")
                sys.exit(1)

        for i in range(10):
            read_line = "Task "+str(j)+"."+str(i)+": read "+str(j)+" from stream."

            if not read_line in lines:
                sys.stderr.write("Could not find line '"+read_line+"' in output.\n")
                sys.exit(1)

            if lines.index(read_line) < lines.index(write_line):
                sys.stderr.write("Read before write for task "+str(i)+".\n")
                sys.exit(1)
