#!/usr/bin/env python

import sys

output_file = sys.argv[1]

with open(output_file) as f:
    lines = map(lambda x: x.strip(), f.readlines())

    for i in range(16):
        if i % 2 == 0:
            if i % 4 == 0:
                read_line = "Consumer A at iteration "+str(i)+" receives: "+str(2*i)+" "+str(2*i+1)
                req_write_line = "Producer at iteration "+str(i)+" sends: "+str(2*i)+" "+str(2*i+1)
            else:
                read_line = "Consumer A at iteration "+str(i)+" receives: "+str(2*i+2)+" "+str(2*i+2+1)
                req_write_line = "Producer at iteration "+str(i+1)+" sends: "+str(2*i+2)+" "+str(2*i+2+1)

            if not read_line in lines:
                sys.stderr.write("Could not find line '"+read_line+"' in output.\n");
                sys.exit(1)

            if not req_write_line in lines:
                sys.stderr.write("Could not find line '"+write_line+"' in output.\n");
                sys.exit(1)

            if lines.index(read_line) < lines.index(req_write_line):
                sys.stderr.write("Read before write for task "+str(i)+".\n")
                sys.exit(1)

        if i % 4 == 0:
            read_line = "Consumer B at iteration "+str(i)+" receives: "+str(2*i+2)+" "+str(2*i+2+1)+" "+str(2*i+2+2)+" "+str(2*i+2+3)
            req_write_lines = [ "Producer at iteration "+str(i+1)+" sends: "+str(2*i+2)+" "+str(2*i+2+1),
                                "Producer at iteration "+str(i+2)+" sends: "+str(2*i+2+2)+" "+str(2*i+2+3) ]

            if not read_line in lines:
                sys.stderr.write("Could not find line '"+read_line+"' in output.\n");

            for write_line in req_write_lines:
                if not write_line in lines:
                    sys.stderr.write("Could not find line '"+write_line+"' in output.\n");
                    sys.exit(1)

                if lines.index(read_line) < lines.index(write_line):
                    sys.stderr.write("Read before write for task "+str(i)+".\n")
                    sys.exit(1)
