#!/usr/bin/env python

import sys

output_file = sys.argv[1]

with open(output_file) as f:
    lines = map(lambda x: x.strip(), f.readlines())

    for i in range(5):
        write_lines = [ "Producer for iteration "+str(2*i)+" sends: "+str(4*i)+" "+str(4*i+1),
                        "Producer for iteration "+str(2*i+1)+" sends: "+str(4*i+2)+" "+str(4*i+3) ]
        read_line = "Consumer for iteration "+str(2*i+1)+" receives: "+str(4*i)+" "+str(4*i+1)+" "+str(4*i+2)+" "+str(4*i+3)

        for write_line in write_lines:
            if not write_line in lines:
                sys.stderr.write("Could not find line '"+write_line+"' in output.\n")
                sys.exit(1)

        if not read_line in lines:
            sys.stderr.write("Could not find line '"+read_line+"' in output.\n")
            sys.exit(1)

        if lines.index(read_line) < lines.index(write_lines[0]) or \
           lines.index(read_line) < lines.index(write_lines[1]):
            sys.stderr.write("Read before write for task "+str(i)+".\n")
            sys.exit(1)
