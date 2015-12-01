#!/usr/bin/env python

import sys

output_file = sys.argv[1]

with open(output_file) as f:
    lines = map(lambda x: x.strip(), f.readlines())

    for i in range(10):
        if i % 2 == 0:
            read_line = "Consumer receives: ("+str(i)+", "+str(2*i)+", "+str(i+1)+", "+str(2*(i+1))+") on stream sin[0] and ("+str(3*i)+", "+str(4*i)+", "+str(3*(i+1))+", "+str(4*(i+1))+") on stream sin[1]."

            req_write_lines = [ "Producer sends: ("+str(i)+", "+str(2*i)+") on stream sout[0] and ("+str(3*i)+", "+str(4*i)+") on stream sout[1].",
                                "Producer sends: ("+str(i+1)+", "+str(2*(i+1))+") on stream sout[0] and ("+str(3*(i+1))+", "+str(4*(i+1))+") on stream sout[1]." ]

            if not read_line in lines:
                sys.stderr.write("Could not find line '"+read_line+"' in output.\n")
                sys.exit(1)

            for write_line in req_write_lines:
                if not write_line in lines:
                    sys.stderr.write("Could not find line '"+write_line+"' in output.\n")
                    sys.exit(1)

                if lines.index(read_line) < lines.index(write_line):
                    sys.stderr.write("Read before write for task "+str(i)+".\n")
                    sys.exit(1)
