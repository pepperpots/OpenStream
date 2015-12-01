#!/usr/bin/env python

import sys

output_file = sys.argv[1]

with open(output_file) as f:
    lines = map(lambda x: x.strip(), f.readlines())
    write_line = "Init task: val = 12345"

    if not write_line in lines:
        sys.stderr.write("Could not find line '"+write_line+"' in output.\n")
        sys.exit(1)

    order = [ "Init task: val = 12345" ]
    order += [ "Middle task "+str(i)+": val = 12345" for i in range(8)]
    order += [ "Terminal task: val = 12345" ]

    for i, l in enumerate(order):
        if not l in lines:
            sys.stderr.write("Could not find line '"+l+"' in output.\n")
            sys.exit(1)

        for j in range(i):
            if lines.index(l) < lines.index(order[j]):
                sys.stderr.write("Read before write for output line task "+str(i)+".\n")
                sys.exit(1)
