#!/usr/bin/python

import sys
from seidel_positions import *

def otherdim_prefix(dim, dim_names):
    ret = ""

    for other_dim in range(len(dim_names)):
        if dim != other_dim:
            ret = ret + dim_names[other_dim]

    return ret

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
        sys.stdout.write(indent+"for(int "+config["dim_names"][dim]+" = 1; "+config["dim_names"][dim]+" < N_"+config["dim_names"][dim]+"+1; "+config["dim_names"][dim]+"++) {\n")

    sys.stdout.write(indent + "\tpos = 0")

    for dim in range(config["num_dims"]):
        sys.stdout.write(" + "+config["dim_names"][dim])
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*(N_"+config["dim_names"][dim_other]+"+2)")
    sys.stdout.write(";\n\n")

    sys.stdout.write(indent + "\tfprintf(fp, \"%f\\t\", matrix[pos]);\n");

    for dim in range(config["num_dims"]):
        sys.stdout.write(indent+"}\n");
        indent = indent[:len(indent)-1]

        if dim != config["num_dims"]-1:
            sys.stdout.write("\n"+indent + "\tfprintf(fp, \"\\n\");\n");

    sys.stdout.write("}\n");

def dump_global_defs(config):
    sys.stdout.write("#include <stdio.h>\n")
    sys.stdout.write("#include <stdlib.h>\n")
    sys.stdout.write("#include <getopt.h>\n")
    sys.stdout.write("#include <string.h>\n")
    sys.stdout.write("#include \"../common/common.h\"\n")
    sys.stdout.write("#include \"../common/sync.h\"\n\n")

def dump_main_fun(config):
    sys.stdout.write("int main(int argc, char** argv)\n")
    sys.stdout.write("{\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("	int N_"+config["dim_names"][dim]+" = -1;\n")

    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("	int block_size_"+config["dim_names"][dim]+" = -1;\n")

    sys.stdout.write("\n")

    sys.stdout.write("	int numiters = -1;\n")
    sys.stdout.write("\n")
    sys.stdout.write("	struct timeval start;\n")
    sys.stdout.write("	struct timeval end;\n")
    sys.stdout.write("\n")
    sys.stdout.write("	int option;\n")
    sys.stdout.write("	FILE* res_file = NULL;\n")
    sys.stdout.write("\n")
    sys.stdout.write("	/* Parse options */\n")
    sys.stdout.write("	while ((option = getopt(argc, argv, \"n:m:s:b:r:o:h\")) != -1)\n")
    sys.stdout.write("	{\n")
    sys.stdout.write("		switch(option)\n")
    sys.stdout.write("		{\n")
    sys.stdout.write("			case 'n':\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("				if(optarg[0] == '"+config["dim_names"][dim]+"')\n")
        sys.stdout.write("					N_"+config["dim_names"][dim]+" = atoi(optarg+1);\n")

    sys.stdout.write("				break;\n")
    sys.stdout.write("			case 's':\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("				if(optarg[0] == '"+config["dim_names"][dim]+"')\n")
        sys.stdout.write("					N_"+config["dim_names"][dim]+" = 1 << atoi(optarg+1);\n")

    sys.stdout.write("				break;\n")
    sys.stdout.write("			case 'b':\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("				if(optarg[0] == '"+config["dim_names"][dim]+"')\n")
        sys.stdout.write("					block_size_"+config["dim_names"][dim]+" = 1 << atoi (optarg+1);\n")

    sys.stdout.write("				break;\n")
    sys.stdout.write("			case 'm':\n")
    for dim in range(config["num_dims"]):
        sys.stdout.write("				if(optarg[0] == '"+config["dim_names"][dim]+"')\n")
        sys.stdout.write("					block_size_"+config["dim_names"][dim]+" = atoi(optarg+1);\n")

    sys.stdout.write("				break;\n")
    sys.stdout.write("			case 'r':\n")
    sys.stdout.write("				numiters = atoi (optarg);\n")
    sys.stdout.write("				break;\n")
    sys.stdout.write("			case 'o':\n")
    sys.stdout.write("				res_file = fopen(optarg, \"w\");\n")
    sys.stdout.write("				break;\n")
    sys.stdout.write("			case 'h':\n")
    sys.stdout.write("				printf(\"Usage: %s [option]...\\n\\n\"\n")
    sys.stdout.write("				       \"Options:\\n\"\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("				       \"  -n "+config["dim_names"][dim]+"<size>                   Number of elements in "+config["dim_names"][dim]+"-direction of the matrix\\n\"\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("				       \"  -s "+config["dim_names"][dim]+"<power>                  Set number of elements in "+config["dim_names"][dim]+"-direction of the matrix to 1 << <power>\\n\"\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("				       \"  -m "+config["dim_names"][dim]+"<size>                   Number of elements in "+config["dim_names"][dim]+"-direction of a block\\n\"\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("				       \"  -b "+config["dim_names"][dim]+"<block size power>       Set the block size in "+config["dim_names"][dim]+"-direction to 1 << <block size power>\\n\"\n")

    sys.stdout.write("				       \"  -r <iterations>              Number of iterations\\n\"\n")
    sys.stdout.write("				       \"  -o <output file>             Write data to output file\\n\",\n")
    sys.stdout.write("				       argv[0]);\n")
    sys.stdout.write("				exit(0);\n")
    sys.stdout.write("				break;\n")
    sys.stdout.write("			case '?':\n")
    sys.stdout.write("				fprintf(stderr, \"Run %s -h for usage.\\n\", argv[0]);\n")
    sys.stdout.write("				exit(1);\n")
    sys.stdout.write("				break;\n")
    sys.stdout.write("		}\n")
    sys.stdout.write("	}\n")
    sys.stdout.write("\n")
    sys.stdout.write("	if(optind != argc) {\n")
    sys.stdout.write("		fprintf(stderr, \"Too many arguments. Run %s -h for usage.\\n\", argv[0]);\n")
    sys.stdout.write("		exit(1);\n")
    sys.stdout.write("	}\n")
    sys.stdout.write("\n")

    sys.stdout.write("	if(numiters == -1) {\n")
    sys.stdout.write("		fprintf(stderr, \"Please set the size of iterations (using the -r switch).\\n\");\n")
    sys.stdout.write("		exit(1);\n")
    sys.stdout.write("	}\n")
    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("	if(N_"+config["dim_names"][dim]+" == -1) {\n")
        sys.stdout.write("		fprintf(stderr, \"Please set the size of the matrix in "+config["dim_names"][dim]+"-direction (using the -n or -s switch).\\n\");\n")
        sys.stdout.write("		exit(1);\n")
        sys.stdout.write("	}\n")
        sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("	if(block_size_"+config["dim_names"][dim]+" == -1) {\n")
        sys.stdout.write("		fprintf(stderr, \"Please set the block size of the matrix in "+config["dim_names"][dim]+"-direction (using the -m or -b switch).\\n\");\n")
        sys.stdout.write("		exit(1);\n")
        sys.stdout.write("	}\n")
        sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("	if(N_"+config["dim_names"][dim]+" % block_size_"+config["dim_names"][dim]+" != 0) {\n")
        sys.stdout.write("		fprintf(stderr, \"Block size in "+config["dim_names"][dim]+"-direction (%d) does not divide size of the matrix in "+config["dim_names"][dim]+"-direction (%d).\\n\", block_size_"+config["dim_names"][dim]+", N_"+config["dim_names"][dim]+");\n")
        sys.stdout.write("		exit(1);\n")
        sys.stdout.write("	}\n")
        sys.stdout.write("\n")

    sys.stdout.write("	size_t matrix_size = sizeof(double)")
    for dim in range(config["num_dims"]):
        sys.stdout.write("*(N_"+config["dim_names"][dim]+"+2)")
    sys.stdout.write(";\n")

    sys.stdout.write("	double* matrix = malloc(matrix_size);\n")

    sys.stdout.write("	memset(matrix, 0, matrix_size);\n\n")

    indent = ""
    for dim in range(config["num_dims"]):
        indent = indent + "\t"
        sys.stdout.write(indent+"for(int "+config["dim_names"][dim]+" = 0; "+config["dim_names"][dim]+" < N_"+config["dim_names"][dim]+"; "+config["dim_names"][dim]+"++) {\n")

    indent = indent + "\t"

    sys.stdout.write(indent + "size_t pos = 0")

    for dim in range(config["num_dims"]):
        sys.stdout.write(" + ("+config["dim_names"][dim]+"+1)")
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*(N_"+config["dim_names"][dim_other]+"+2)")
    sys.stdout.write(";\n\n")

    num = 0
    for init_pos in config["init_positions"]:
        if num == 0:
            sys.stdout.write(indent + "if(1")
        else:
            sys.stdout.write(indent + "else if(1")

        for idx in range(config["num_dims"]):
            sys.stdout.write(" && "+config["dim_names"][idx] + " == "+init_pos["pos"][idx])
        sys.stdout.write(")\n")
        sys.stdout.write(indent + "\tmatrix[pos] = "+init_pos["val"]+";\n")
        num = 1

    for dim in range(config["num_dims"]):
        indent = indent[:len(indent)-1]
        sys.stdout.write(indent+"}\n");

    sys.stdout.write("\n")

    sys.stdout.write("	gettimeofday(&start, NULL);\n")
    sys.stdout.write("\n")

    sys.stdout.write("\tfor(int iter = 0; iter < numiters; iter++) {\n")

    indent = "\t"
    for dim in range(config["num_dims"]):
        indent = indent + "\t"
        sys.stdout.write(indent+"for(int "+config["dim_names"][dim]+config["dim_names"][dim]+" = 1; "+config["dim_names"][dim]+config["dim_names"][dim]+" < N_"+config["dim_names"][dim]+"+1; "+config["dim_names"][dim]+config["dim_names"][dim]+" += block_size_"+config["dim_names"][dim]+") {\n")

    for dim in range(config["num_dims"]):
        indent = indent + "\t"
        sys.stdout.write(indent+"for(int "+config["dim_names"][dim]+" = "+config["dim_names"][dim]+config["dim_names"][dim]+"; "+config["dim_names"][dim]+" < "+config["dim_names"][dim]+config["dim_names"][dim]+" + block_size_"+config["dim_names"][dim]+"; "+config["dim_names"][dim]+"++) {\n")

    center_expr = "0";

    for dim_other in range(config["num_dims"]):
        center_expr += " + "+config["dim_names"][dim_other]

        for dim_other_t in range(dim_other+1, config["num_dims"]):
                center_expr += "*(N_"+config["dim_names"][dim_other_t]+"+2)"

    sys.stdout.write(indent+"\tmatrix["+center_expr+"] = (matrix["+center_expr+"]")

    for dim in range(config["num_dims"]):
        for direction in range(2):
            sys.stdout.write(" +\n"+indent+"\t\tmatrix[0")
            for dim_other in range(config["num_dims"]):
                if dim_other == dim:
                    if direction == 0:
                        sys.stdout.write(" + ("+config["dim_names"][dim_other]+"-1)")
                    else:
                        sys.stdout.write(" + ("+config["dim_names"][dim_other]+"+1)")
                else:
                    sys.stdout.write(" + "+config["dim_names"][dim_other]+"")

                for dim_other_t in range(dim_other+1, config["num_dims"]):
                    sys.stdout.write("*(N_"+config["dim_names"][dim_other_t]+"+2)")
            sys.stdout.write("]")

    sys.stdout.write(") / "+str(2*config["num_dims"]+1)+".0;\n")

    for dim in range(2*config["num_dims"]):
        indent = indent[:len(indent)-1]
        sys.stdout.write(indent+"\t}\n");

    sys.stdout.write("\t}\n\n")

    sys.stdout.write("	gettimeofday(&end, NULL);\n")
    sys.stdout.write("\n")
    sys.stdout.write("	printf(\"%.5f\\n\", tdiff(&end, &start));\n")
    sys.stdout.write("\n")

    sys.stdout.write("	if(res_file) {\n")
    sys.stdout.write("	\tdump_matrix_%dd(matrix, res_file"%config["num_dims"])
    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim])
    sys.stdout.write(");\n")
    sys.stdout.write("	\tfclose(res_file);\n")
    sys.stdout.write("	}\n\n")

    sys.stdout.write("	free(matrix);\n")

    sys.stdout.write("\n")
    sys.stdout.write("	return 0;\n")
    sys.stdout.write("}")

def dump_file(config):
    dump_global_defs(config)
    sys.stdout.write("\n");

    dump_dump_matrix_fun_signature(config)
    sys.stdout.write(";\n\n");

    dump_dump_matrix_fun(config)
    sys.stdout.write("\n")

    dump_main_fun(config)
    sys.stdout.write("\n");
