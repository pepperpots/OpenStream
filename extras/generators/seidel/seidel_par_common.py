#!/usr/bin/python

import sys

def dump_bodyfuncs_global_defs(config):
    sys.stdout.write("#include <stdio.h>\n\n")

def dump_global_defs(config):
    sys.stdout.write("#include <stdio.h>\n")
    sys.stdout.write("#include <stdlib.h>\n")
    sys.stdout.write("#include <getopt.h>\n")
    sys.stdout.write("#include <string.h>\n")
    sys.stdout.write("#include \"../common/common.h\"\n")
    sys.stdout.write("#include \"../common/sync.h\"\n\n")

    sys.stdout.write("void* sdfbarrier_ref;\n")
    sys.stdout.write("void* scenter_ref;\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("void* s"+config["dimref_names"][dim][0]+"_ref;\n")
        sys.stdout.write("void* s"+config["dimref_names"][dim][1]+"_ref;\n")

def dump_dump_matrix_fun_signature(config):
    sys.stdout.write("void dump_matrix_%dd(double* matrix, FILE* fp"%config["num_dims"]);

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int N_"+config["dim_names"][dim])

    sys.stdout.write(")");

def dump_dump_matrix_fun(config):
    dump_dump_matrix_fun_signature(config)
    sys.stdout.write("\n{\n");
    sys.stdout.write("\tsize_t pos;\n\n");

    indent = ""
    for dim in range(config["num_dims"]):
        indent = indent + "\t"
        sys.stdout.write(indent+"for(int "+config["dim_names"][dim]+" = 0; "+config["dim_names"][dim]+" < N_"+config["dim_names"][dim]+"; "+config["dim_names"][dim]+"++) {\n")

    sys.stdout.write(indent + "\tpos = 0")

    for dim in range(config["num_dims"]):
        sys.stdout.write(" + "+config["dim_names"][dim])
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*N_"+config["dim_names"][dim_other])
    sys.stdout.write(";\n\n")

    sys.stdout.write(indent + "\tfprintf(fp, \"%f\\t\", matrix[pos]);\n");

    for dim in range(config["num_dims"]):
        sys.stdout.write(indent+"}\n");
        indent = indent[:len(indent)-1]

        if dim != config["num_dims"]-1:
            sys.stdout.write("\n"+indent + "\tfprintf(fp, \"\\n\");\n");

    sys.stdout.write("}\n");
