#!/usr/bin/python

import sys
from jacobi_positions import *

def dump_global_defs(config):
    sys.stdout.write("#include <stdio.h>\n")
    sys.stdout.write("#include <stdlib.h>\n")
    sys.stdout.write("#include <getopt.h>\n")
    sys.stdout.write("#include <string.h>\n")
    sys.stdout.write("#include \"../common/common.h\"\n")
    sys.stdout.write("#include \"../common/sync.h\"\n\n")

    if config["with_output"]:
        sys.stdout.write("#define _WITH_OUTPUT 1\n")
    else:
        sys.stdout.write("#define _WITH_OUTPUT 0\n")

    if config["with_binary_output"]:
        sys.stdout.write("#define _WITH_BINARY_OUTPUT 1\n")
    else:
        sys.stdout.write("#define _WITH_BINARY_OUTPUT 0\n")

    sys.stdout.write("\n")

    sys.stdout.write("void* sdfbarrier_ref;\n")
    sys.stdout.write("void* scenter_ref;\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("void* s"+config["dimref_names"][dim][0]+"_ref;\n")
        sys.stdout.write("void* s"+config["dimref_names"][dim][1]+"_ref;\n")

def dump_terminal_task(config, pref):
    if config["debug_printf"]:
        sys.stdout.write(pref+"printf(\"Creating terminal task for (")
        for dim in range(config["num_dims"]):
            sys.stdout.write("%d ");
        sys.stdout.write(") id = %d\\n\"");

        for dim in range(config["num_dims"]):
            sys.stdout.write(", id_"+config["dim_names"][dim])
        sys.stdout.write(", id);\n");

    sys.stdout.write(pref+"#pragma omp task \\\n")

    num_clauses = 0
    sys.stdout.write(pref+"\tinput(")
    sys.stdout.write("scenter_ref[id] >> center_in[center_in_size]")

    for dim in range(config["num_dims"]):
        if config["dim_configs"][dim] != jacobi_positions.first and config["dim_configs"][dim] != jacobi_positions.span:
            sys.stdout.write(", \\\n"+pref+"\t\t")
            sys.stdout.write("s"+config["dimref_names"][dim][0]+"_ref[id] >> "+config["dimref_names"][dim][0]+"_in["+config["dimref_names"][dim][0]+"_in_size]")
            num_clauses = num_clauses + 1

        if config["dim_configs"][dim] != jacobi_positions.last and config["dim_configs"][dim] != jacobi_positions.span:
            sys.stdout.write(", \\\n"+pref+"\t\t")
            sys.stdout.write("s"+config["dimref_names"][dim][1]+"_ref[id] >> "+config["dimref_names"][dim][1]+"_in["+config["dimref_names"][dim][0]+"_in_size]")
            num_clauses = num_clauses + 1

    sys.stdout.write(") \\\n")
    sys.stdout.write(pref+"\toutput(sdfbarrier_ref[0] << token[1])\n")
    sys.stdout.write(pref+"{\n")
    sys.stdout.write(pref+"\tjacobi_%dd_df_finish(matrix"%config["num_dims"])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", id_"+config["dim_names"][dim])

    sys.stdout.write(", center_in);\n");

    sys.stdout.write(pref+"}\n\n")

def dump_create_terminal_task_fun_signature(config):
    sys.stdout.write("void create_terminal_task(double* matrix, int numiters")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int id_"+config["dim_names"][dim])

    sys.stdout.write(")")

def dump_create_terminal_task_fun(config):
    dump_create_terminal_task_fun_signature(config)
    sys.stdout.write("\n{\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("\tint blocks_"+config["dim_names"][dim]+" = N_"+config["dim_names"][dim]+" / block_size_"+config["dim_names"][dim]+";\n")

    sys.stdout.write("\tint blocks = 1")
    for dim in range(config["num_dims"]):
        sys.stdout.write("*blocks_"+config["dim_names"][dim]);
    sys.stdout.write(";\n")

    sys.stdout.write("\tint id = numiters*blocks")

    for dim in range(config["num_dims"]):
        sys.stdout.write(" + id_"+config["dim_names"][dim]);
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*blocks_"+config["dim_names"][dim_other])
    sys.stdout.write(";\n")

    sys.stdout.write("\tint token[1];\n")

    sz = "1"
    for dim in range(config["num_dims"]):
        sz = sz + "*block_size_"+config["dim_names"][dim]

    sys.stdout.write("\tint center_in_size = "+sz+";\n")
    sys.stdout.write("\tdouble center_in[center_in_size];\n")

    for dim in range(config["num_dims"]):
        sz = "1"

        for dim_other in range(config["num_dims"]):
            if dim_other != dim:
                sz = sz + "*block_size_"+config["dim_names"][dim_other]

        sys.stdout.write("\tint "+config["dimref_names"][dim][0]+"_in_size = "+sz+";\n")
        sys.stdout.write("\tint "+config["dimref_names"][dim][1]+"_in_size = "+sz+";\n")
        sys.stdout.write("\tdouble "+config["dimref_names"][dim][0]+"_in["+config["dimref_names"][dim][0]+"_in_size];\n")
        sys.stdout.write("\tdouble "+config["dimref_names"][dim][1]+"_in["+config["dimref_names"][dim][0]+"_in_size];\n")

    for config_nr in range(4**config["num_dims"]):
        config["dim_configs"] = []

        for dim in range(config["num_dims"]):
            config["dim_configs"].append((config_nr / 4**dim) % 4)

        sys.stdout.write("\n\tif(1")
        for dim in range(config["num_dims"]):
            if(config["dim_configs"][dim] == jacobi_positions.first):
                sys.stdout.write(" && id_"+config["dim_names"][dim]+" == 0 && blocks_"+config["dim_names"][dim]+" > 1")
            elif(config["dim_configs"][dim] == jacobi_positions.mid):
                sys.stdout.write(" && id_"+config["dim_names"][dim]+" > 0 && id_"+config["dim_names"][dim]+" < blocks_"+config["dim_names"][dim]+"-1")
            elif(config["dim_configs"][dim] == jacobi_positions.last):
                sys.stdout.write(" && id_"+config["dim_names"][dim]+" == blocks_"+config["dim_names"][dim]+"-1 && blocks_"+config["dim_names"][dim]+" > 1")
            elif(config["dim_configs"][dim] == jacobi_positions.span):
                sys.stdout.write(" && blocks_"+config["dim_names"][dim]+" == 1")
        sys.stdout.write(") {\n")
        dump_terminal_task(config, "\t\t")
        sys.stdout.write("\t}\n")

    sys.stdout.write("}\n")

def dump_finish_fun_signature(config):
    sys.stdout.write("void jacobi_%dd_df_finish(double* matrix"%config["num_dims"])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int id_"+config["dim_names"][dim])

    sys.stdout.write(", double* center_in)")

def dump_finish_fun(config):
    dump_finish_fun_signature(config)
    sys.stdout.write("\n{\n");

    indent = "\t"

    for dim in range(config["num_dims"]):
        sys.stdout.write(indent+"for(int "+config["dim_names"][dim]+" = 0; "+config["dim_names"][dim]+" < block_size_"+config["dim_names"][dim]+"; "+config["dim_names"][dim]+"++) {\n")
        indent = indent+"\t"

    for dim in range(config["num_dims"]):
        sys.stdout.write(indent+"int global_"+config["dim_names"][dim]+" = id_"+config["dim_names"][dim]+" * block_size_"+config["dim_names"][dim]+" + "+config["dim_names"][dim]+";\n")

    sys.stdout.write("\n")
    sys.stdout.write(indent+"int local_index = 0")
    for dim in range(config["num_dims"]):
        sys.stdout.write(" + "+config["dim_names"][dim]+"")
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*block_size_"+config["dim_names"][dim_other])
    sys.stdout.write(";\n")

    sys.stdout.write(indent+"int global_index = 0")
    for dim in range(config["num_dims"]):
        sys.stdout.write(" + global_"+config["dim_names"][dim]+"")
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*N_"+config["dim_names"][dim_other])
    sys.stdout.write(";\n\n")

    sys.stdout.write(indent+"matrix[global_index] = center_in[local_index];\n")

    for dim in range(config["num_dims"]):
        indent = indent[:len(indent)-1]
        sys.stdout.write(indent+"}\n");

    sys.stdout.write("}\n");

def dump_init_task(config, pref):
    if config["debug_printf"]:
        sys.stdout.write(pref+"printf(\"Creating init task for (")
        for dim in range(config["num_dims"]):
            sys.stdout.write("%d ");
        sys.stdout.write(") id = %d\\n\"");

        for dim in range(config["num_dims"]):
            sys.stdout.write(", id_"+config["dim_names"][dim])
        sys.stdout.write(", id);\n");

    sys.stdout.write(pref+"#pragma omp task \\\n")

    num_clauses = 0
    sys.stdout.write(pref+"\toutput(")
    sys.stdout.write("scenter_ref[id] << center_out[center_out_size]")

    for dim in range(config["num_dims"]):
        if config["dim_configs"][dim] != jacobi_positions.first and config["dim_configs"][dim] != jacobi_positions.span:
            sys.stdout.write(", \\\n"+pref+"\t\t")
            sys.stdout.write("s"+config["dimref_names"][dim][0]+"_ref[id] << "+config["dimref_names"][dim][0]+"_out["+config["dimref_names"][dim][0]+"_out_size]")
            num_clauses = num_clauses + 1

        if config["dim_configs"][dim] != jacobi_positions.last and config["dim_configs"][dim] != jacobi_positions.span:
            sys.stdout.write(", \\\n"+pref+"\t\t")
            sys.stdout.write("s"+config["dimref_names"][dim][1]+"_ref[id] << "+config["dimref_names"][dim][1]+"_out["+config["dimref_names"][dim][0]+"_out_size]")
            num_clauses = num_clauses + 1

    sys.stdout.write(")\n")
    sys.stdout.write(pref+"{\n")
    sys.stdout.write(pref+"\tjacobi_%dd_df_init(matrix"%config["num_dims"])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", id_"+config["dim_names"][dim])

    sys.stdout.write(", center_out")

    for dim in range(config["num_dims"]):
        if config["dim_configs"][dim] != jacobi_positions.first and config["dim_configs"][dim] != jacobi_positions.span:
            sys.stdout.write(", "+config["dimref_names"][dim][0]+"_out")
        else:
            sys.stdout.write(", NULL")

        if config["dim_configs"][dim] != jacobi_positions.last and config["dim_configs"][dim] != jacobi_positions.span:
            sys.stdout.write(", "+config["dimref_names"][dim][1]+"_out")
        else:
            sys.stdout.write(", NULL")

    sys.stdout.write(");\n");
    sys.stdout.write(pref+"\tcreate_init_followup_task(matrix, numiters")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", id_"+config["dim_names"][dim])

    sys.stdout.write(");\n")

    sys.stdout.write(pref+"}\n")

def dump_create_initial_task_fun_signature(config):
    sys.stdout.write("void create_initial_task(double* matrix, int numiters")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int id_"+config["dim_names"][dim])

    sys.stdout.write(")")

def dump_create_initial_task_fun(config):
    dump_create_initial_task_fun_signature(config)
    sys.stdout.write("\n{\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("\tint blocks_"+config["dim_names"][dim]+" = N_"+config["dim_names"][dim]+" / block_size_"+config["dim_names"][dim]+";\n")

    sys.stdout.write("\tint id = 0")

    for dim in range(config["num_dims"]):
        sys.stdout.write(" + id_"+config["dim_names"][dim]);
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*blocks_"+config["dim_names"][dim_other])
    sys.stdout.write(";\n")

    sz = "1"
    for dim in range(config["num_dims"]):
        sz = sz + "*block_size_"+config["dim_names"][dim]

    sys.stdout.write("\tint center_out_size = "+sz+";\n")
    sys.stdout.write("\tdouble center_out[center_out_size];\n")

    for dim in range(config["num_dims"]):
        sz = "1"

        for dim_other in range(config["num_dims"]):
            if dim_other != dim:
                sz = sz + "*block_size_"+config["dim_names"][dim_other]

        sys.stdout.write("\tint "+config["dimref_names"][dim][0]+"_out_size = "+sz+";\n")
        sys.stdout.write("\tint "+config["dimref_names"][dim][1]+"_out_size = "+sz+";\n")
        sys.stdout.write("\tdouble "+config["dimref_names"][dim][0]+"_out["+config["dimref_names"][dim][0]+"_out_size];\n")
        sys.stdout.write("\tdouble "+config["dimref_names"][dim][1]+"_out["+config["dimref_names"][dim][0]+"_out_size];\n")

    for config_nr in range(4**config["num_dims"]):
        config["dim_configs"] = []

        for dim in range(config["num_dims"]):
            config["dim_configs"].append((config_nr / 4**dim) % 4)

        sys.stdout.write("\n\tif(1")
        for dim in range(config["num_dims"]):
            if(config["dim_configs"][dim] == jacobi_positions.first):
                sys.stdout.write(" && id_"+config["dim_names"][dim]+" == 0 && blocks_"+config["dim_names"][dim]+" > 1")
            elif(config["dim_configs"][dim] == jacobi_positions.mid):
                sys.stdout.write(" && id_"+config["dim_names"][dim]+" > 0 && id_"+config["dim_names"][dim]+" < blocks_"+config["dim_names"][dim]+"-1")
            elif(config["dim_configs"][dim] == jacobi_positions.last):
                sys.stdout.write(" && id_"+config["dim_names"][dim]+" == blocks_"+config["dim_names"][dim]+"-1 && blocks_"+config["dim_names"][dim]+" > 1")
            elif(config["dim_configs"][dim] == jacobi_positions.span):
                sys.stdout.write(" && blocks_"+config["dim_names"][dim]+" == 1")
        sys.stdout.write(") {\n")
        dump_init_task(config, "\t\t")
        sys.stdout.write("\t}\n")

    sys.stdout.write("}\n")

def dump_init_fun_signature(config):
    sys.stdout.write("void jacobi_%dd_df_init(double* matrix"%config["num_dims"])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int id_"+config["dim_names"][dim])

    sys.stdout.write(", double* center_out")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", double* "+config["dimref_names"][dim][0]+"_out")
        sys.stdout.write(", double* "+config["dimref_names"][dim][1]+"_out")

    sys.stdout.write(")");

def dump_init_fun(config):
    dump_init_fun_signature(config)
    sys.stdout.write("\n{\n");

    indent = "\t"

    for dim in range(config["num_dims"]):
        sys.stdout.write(indent+"for(int "+config["dim_names"][dim]+" = 0; "+config["dim_names"][dim]+" < block_size_"+config["dim_names"][dim]+"; "+config["dim_names"][dim]+"++) {\n")
        indent = indent+"\t"

    for dim in range(config["num_dims"]):
        sys.stdout.write(indent+"int global_"+config["dim_names"][dim]+" = id_"+config["dim_names"][dim]+" * block_size_"+config["dim_names"][dim]+" + "+config["dim_names"][dim]+";\n")

    sys.stdout.write("\n")
    sys.stdout.write(indent+"int local_index = 0")
    for dim in range(config["num_dims"]):
        sys.stdout.write(" + "+config["dim_names"][dim]+"")
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*block_size_"+config["dim_names"][dim_other])
    sys.stdout.write(";\n")

    sys.stdout.write(indent+"int global_index = 0")
    for dim in range(config["num_dims"]):
        sys.stdout.write(" + global_"+config["dim_names"][dim]+"")
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*N_"+config["dim_names"][dim_other])
    sys.stdout.write(";\n\n")


    sys.stdout.write(indent+"double curr = matrix[global_index];\n\n")

    sys.stdout.write(indent+"center_out[local_index] = curr;\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("\n"+indent+"if("+config["dim_names"][dim]+" == 0 && "+config["dimref_names"][dim][0]+"_out)\n")
        sys.stdout.write(indent+"\t"+config["dimref_names"][dim][0]+"_out[0")
        for dim_other in range(0, config["num_dims"]):
            if(dim_other != dim):
                sys.stdout.write(" + "+config["dim_names"][dim_other])
                for dim_other_t in range(dim_other+1, config["num_dims"]):
                    if(dim_other_t != dim):
                        sys.stdout.write("*block_size_"+config["dim_names"][dim_other_t])
        sys.stdout.write("] = curr;\n")

        sys.stdout.write("\n"+indent+"if("+config["dim_names"][dim]+" == block_size_"+config["dim_names"][dim]+"-1 && "+config["dimref_names"][dim][1]+"_out)\n")
        sys.stdout.write(indent+"\t"+config["dimref_names"][dim][1]+"_out[0")
        for dim_other in range(0, config["num_dims"]):
            if(dim_other != dim):
                sys.stdout.write(" + "+config["dim_names"][dim_other])
                for dim_other_t in range(dim_other+1, config["num_dims"]):
                    if(dim_other_t != dim):
                        sys.stdout.write("*block_size_"+config["dim_names"][dim_other_t])
        sys.stdout.write("] = curr;\n")

    for dim in range(config["num_dims"]):
        indent = indent[:len(indent)-1]
        sys.stdout.write(indent+"}\n");

    sys.stdout.write("}\n");

def dump_create_init_followup_task_fun_signature(config):
    sys.stdout.write("void create_init_followup_task(double* matrix, int numiters")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int id_"+config["dim_names"][dim])

    sys.stdout.write(")")

def dump_create_init_followup_task_fun(config):
    dump_create_init_followup_task_fun_signature(config)
    sys.stdout.write("\n{\n")

    sys.stdout.write("\tif(numiters > 1)\n")
    sys.stdout.write("\t\tcreate_next_iteration_task(matrix, numiters, 1")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", id_"+config["dim_names"][dim])
    sys.stdout.write(");\n\n")

    sys.stdout.write("\tif(numiters == 1)\n")
    sys.stdout.write("\t\tcreate_terminal_task(matrix, numiters")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", id_"+config["dim_names"][dim])
    sys.stdout.write(");\n")

    sys.stdout.write("}\n\n")

def dump_create_followup_task_fun_signature(config):
    sys.stdout.write("void create_followup_task(double* matrix, int numiters, int it")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int id_"+config["dim_names"][dim])

    sys.stdout.write(")")

def dump_create_followup_task_fun(config):
    dump_create_followup_task_fun_signature(config)
    sys.stdout.write("\n{\n")

    sys.stdout.write("\tif(it+2 < numiters)\n")
    sys.stdout.write("\t\tcreate_next_iteration_task(matrix, numiters, it+2")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", id_"+config["dim_names"][dim])
    sys.stdout.write(");\n\n")

    sys.stdout.write("\tif(it == numiters - 2 && numiters > 1)\n")
    sys.stdout.write("\t\tcreate_terminal_task(matrix, numiters")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", id_"+config["dim_names"][dim])
    sys.stdout.write(");\n")

    sys.stdout.write("}\n\n")

def dump_next_iteration_task(config, pref):
    if config["debug_printf"]:
        sys.stdout.write(pref+"printf(\"Creating task for (")
        for dim in range(config["num_dims"]):
            sys.stdout.write("%d ");
        sys.stdout.write(") it = %d, id = %d");

        for dim in range(config["num_dims"]):
            if config["dim_configs"][dim] != jacobi_positions.first and config["dim_configs"][dim] != jacobi_positions.span:
                sys.stdout.write(", id_"+config["dimref_names"][dim][0]+" = %d")

            if config["dim_configs"][dim] != jacobi_positions.last and config["dim_configs"][dim] != jacobi_positions.span:
                sys.stdout.write(", id_"+config["dimref_names"][dim][1]+" = %d")

        sys.stdout.write("\\n\"");

        for dim in range(config["num_dims"]):
            sys.stdout.write(", id_"+config["dim_names"][dim]);

        sys.stdout.write(", it, id")

        for dim in range(config["num_dims"]):
            if config["dim_configs"][dim] != jacobi_positions.first and config["dim_configs"][dim] != jacobi_positions.span:
                sys.stdout.write(", id_"+config["dimref_names"][dim][0])

            if config["dim_configs"][dim] != jacobi_positions.last and config["dim_configs"][dim] != jacobi_positions.span:
                sys.stdout.write(", id_"+config["dimref_names"][dim][1])

        sys.stdout.write(");\n");

    sys.stdout.write(pref+"#pragma omp task \\\n")

    num_clauses = 0
    sys.stdout.write(pref+"\tinout_reuse(")
    sys.stdout.write("scenter_ref[id_center] >> center[center_size] >> scenter_ref[id]")

    for dim in range(config["num_dims"]):
        if config["dim_configs"][dim] != jacobi_positions.first and config["dim_configs"][dim] != jacobi_positions.span:
            sys.stdout.write(", \\\n"+pref+"\t\t")
            sys.stdout.write("s"+config["dimref_names"][dim][1]+"_ref[id_"+config["dimref_names"][dim][0]+"] >> "+config["dimref_names"][dim][0]+"["+config["dimref_names"][dim][0]+"_size] >> s"+config["dimref_names"][dim][0]+"_ref[id]")
            num_clauses = num_clauses + 1

        if config["dim_configs"][dim] != jacobi_positions.last and config["dim_configs"][dim] != jacobi_positions.span:
            sys.stdout.write(", \\\n"+pref+"\t\t")
            sys.stdout.write("s"+config["dimref_names"][dim][0]+"_ref[id_"+config["dimref_names"][dim][1]+"] >> "+config["dimref_names"][dim][1]+"["+config["dimref_names"][dim][1]+"_size] >> s"+config["dimref_names"][dim][1]+"_ref[id]")
            num_clauses = num_clauses + 1

    sys.stdout.write(")\n")
    sys.stdout.write(pref+"{\n")

    if config["debug_printf"]:
        sys.stdout.write(pref+"\tprintf(\"Executing task for (")
        for dim in range(config["num_dims"]):
            sys.stdout.write("%d ");
        sys.stdout.write(") it = %d\\n\"");
        for dim in range(config["num_dims"]):
                sys.stdout.write(", id_"+config["dim_names"][dim]);
        sys.stdout.write(", it);\n");

    sys.stdout.write(pref+"\tjacobi_%dd_df_in_place("%config["num_dims"])

    for dim in range(config["num_dims"]):
        if dim != 0:
            sys.stdout.write(", ")
        sys.stdout.write("block_size_"+config["dim_names"][dim])

    sys.stdout.write(", center")

    for dim in range(config["num_dims"]):
        if config["dim_configs"][dim] != jacobi_positions.first and config["dim_configs"][dim] != jacobi_positions.span:
            sys.stdout.write(", "+config["dimref_names"][dim][0])
        else:
            sys.stdout.write(", NULL")

        if config["dim_configs"][dim] != jacobi_positions.last and config["dim_configs"][dim] != jacobi_positions.span:
            sys.stdout.write(", "+config["dimref_names"][dim][1])
        else:
            sys.stdout.write(", NULL")

    sys.stdout.write(");\n");
    sys.stdout.write(pref+"\tcreate_followup_task(matrix, numiters, it")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", id_"+config["dim_names"][dim])

    sys.stdout.write(");\n")

    sys.stdout.write(pref+"}\n")

def dump_create_next_iteration_task_fun_signature(config):
    sys.stdout.write("void create_next_iteration_task(double* matrix, int numiters, int it")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int id_"+config["dim_names"][dim])

    sys.stdout.write(")")

def dump_create_next_iteration_task_fun(config):
    dump_create_next_iteration_task_fun_signature(config)
    sys.stdout.write("\n{\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("\tint blocks_"+config["dim_names"][dim]+" = N_"+config["dim_names"][dim]+" / block_size_"+config["dim_names"][dim]+";\n")

    sys.stdout.write("\tint blocks = 1")
    for dim in range(config["num_dims"]):
        sys.stdout.write("*blocks_"+config["dim_names"][dim]);
    sys.stdout.write(";\n")

    sys.stdout.write("\tint id_center = it*blocks + 0")

    for dim in range(config["num_dims"]):
        sys.stdout.write(" + id_"+config["dim_names"][dim]);
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*blocks_"+config["dim_names"][dim_other])
    sys.stdout.write(";\n")

    sys.stdout.write("\tint id = (it+1)*blocks + 0")

    for dim in range(config["num_dims"]):
        sys.stdout.write(" + id_"+config["dim_names"][dim]);
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*blocks_"+config["dim_names"][dim_other])
    sys.stdout.write(";\n")

    for dim in range(config["num_dims"]):
        for direction in range(2):
            sys.stdout.write("\tint id_"+config["dimref_names"][dim][direction]+" = it*blocks + 0")

            for dim_other in range(config["num_dims"]):
                if dim_other == dim:
                    if direction == 0:
                        sys.stdout.write(" + (id_"+config["dim_names"][dim_other]+"-1)");
                    else:
                        sys.stdout.write(" + (id_"+config["dim_names"][dim_other]+"+1)");
                else:
                    sys.stdout.write(" + id_"+config["dim_names"][dim_other]);

                for dim_other_t in range(dim_other+1, config["num_dims"]):
                    sys.stdout.write("*blocks_"+config["dim_names"][dim_other_t])

            sys.stdout.write(";\n")

    sz = "1"
    for dim in range(config["num_dims"]):
        sz = sz + "*block_size_"+config["dim_names"][dim]

    sys.stdout.write("\tint center_size = "+sz+";\n")
    sys.stdout.write("\tdouble center[center_size];\n")

    for dim in range(config["num_dims"]):
        sz = "1"

        for dim_other in range(config["num_dims"]):
            if dim_other != dim:
                sz = sz + "*block_size_"+config["dim_names"][dim_other]

        sys.stdout.write("\tint "+config["dimref_names"][dim][0]+"_size = "+sz+";\n")
        sys.stdout.write("\tint "+config["dimref_names"][dim][1]+"_size = "+sz+";\n")
        sys.stdout.write("\tdouble "+config["dimref_names"][dim][0]+"["+config["dimref_names"][dim][0]+"_size];\n")
        sys.stdout.write("\tdouble "+config["dimref_names"][dim][1]+"["+config["dimref_names"][dim][0]+"_size];\n")


    for config_nr in range(4**config["num_dims"]):
        config["dim_configs"] = []

        for dim in range(config["num_dims"]):
            config["dim_configs"].append((config_nr / 4**dim) % 4)

        sys.stdout.write("\n\tif(1")
        for dim in range(config["num_dims"]):
            if(config["dim_configs"][dim] == jacobi_positions.first):
                sys.stdout.write(" && id_"+config["dim_names"][dim]+" == 0 && blocks_"+config["dim_names"][dim]+" > 1")
            elif(config["dim_configs"][dim] == jacobi_positions.mid):
                sys.stdout.write(" && id_"+config["dim_names"][dim]+" > 0 && id_"+config["dim_names"][dim]+" < blocks_"+config["dim_names"][dim]+"-1")
            elif(config["dim_configs"][dim] == jacobi_positions.last):
                sys.stdout.write(" && id_"+config["dim_names"][dim]+" == blocks_"+config["dim_names"][dim]+"-1 && blocks_"+config["dim_names"][dim]+" > 1")
            elif(config["dim_configs"][dim] == jacobi_positions.span):
                sys.stdout.write(" && blocks_"+config["dim_names"][dim]+" == 1")
        sys.stdout.write(") {\n")
        dump_next_iteration_task(config, "\t\t")
        sys.stdout.write("\t}\n")

    sys.stdout.write("}\n")

def dump_jacobi_fun_signature(config):
    sys.stdout.write("void jacobi_%dd_df_in_place("%config["num_dims"])

    for dim in range(config["num_dims"]):
        if dim != 0:
            sys.stdout.write(", ")
        sys.stdout.write("int block_size_"+config["dim_names"][dim])

    sys.stdout.write(", double* center")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", double* "+config["dimref_names"][dim][0])
        sys.stdout.write(", double* "+config["dimref_names"][dim][1])

    sys.stdout.write(")");

def otherdim_prefix(dim, dim_names):
    ret = ""

    for other_dim in range(len(dim_names)):
        if dim != other_dim:
            ret = ret + dim_names[other_dim]

    return ret

def dump_jacobi_fun(config):
    dump_jacobi_fun_signature(config)
    sys.stdout.write("\n{\n");

    for dim in range(config["num_dims"]):
        sys.stdout.write("\tint "+config["dim_names"][dim]+"_dir = ("+config["dimref_names"][dim][0]+") ? 1 : -1;\n")

    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("\tdouble* "+otherdim_prefix(dim, config["dim_names"])+"_saver = ("+config["dim_names"][dim]+"_dir == 1) ? "+config["dimref_names"][dim][0]+" : "+config["dimref_names"][dim][1]+";\n")

    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("\tint "+config["dim_names"][dim]+"_start = ("+config["dim_names"][dim]+"_dir == 1) ? 0 : block_size_"+config["dim_names"][dim]+" - 1;\n")

    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("\tint "+config["dim_names"][dim]+"_end = ("+config["dim_names"][dim]+"_dir == 1) ? block_size_"+config["dim_names"][dim]+" : -1;\n")

    sys.stdout.write("\n")


    indent = "\t"

    for dim in range(config["num_dims"]):
        sys.stdout.write(indent+"for(int "+config["dim_names"][dim]+" = "+config["dim_names"][dim]+"_start; "+config["dim_names"][dim]+" != "+config["dim_names"][dim]+"_end; "+config["dim_names"][dim]+" += "+config["dim_names"][dim]+"_dir) {\n")
        indent = indent+"\t"

    sys.stdout.write(indent+"int index_center = 0")
    for dim in range(config["num_dims"]):
        sys.stdout.write(" + "+config["dim_names"][dim]+"")
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*block_size_"+config["dim_names"][dim_other])
    sys.stdout.write(";\n")

    sys.stdout.write("\n")
    for dim in range(config["num_dims"]):
        sys.stdout.write(indent+"int "+otherdim_prefix(dim, config["dim_names"])+"_saver_index = 0")

        for dim_other in range(config["num_dims"]):
            if dim_other != dim:
                sys.stdout.write(" + "+config["dim_names"][dim_other]);
                for dim_other_t in range(dim_other + 1, config["num_dims"]):
                    if dim_other_t != dim_other and dim_other_t != dim:
                        sys.stdout.write("*block_size_"+config["dim_names"][dim_other_t]);
        sys.stdout.write(";\n");

    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        for direction in range(0, 2):
            sys.stdout.write(indent+"int index_"+config["dimref_names"][dim][direction]+"_center = 0")
            for dim_other in range(config["num_dims"]):
                if dim_other == dim:
                    if direction == 0:
                        sys.stdout.write(" + ("+config["dim_names"][dim_other]+"-1)")
                    else:
                        sys.stdout.write(" + ("+config["dim_names"][dim_other]+"+1)")
                else:
                    sys.stdout.write(" + "+config["dim_names"][dim_other]+"")

                for dim_other_t in range(dim_other+1, config["num_dims"]):
                    sys.stdout.write("*block_size_"+config["dim_names"][dim_other_t])
            sys.stdout.write(";\n")

    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        for direction in range(0, 2):
            sys.stdout.write(indent+"int index_"+config["dimref_names"][dim][direction]+" = 0")
            for dim_other in range(config["num_dims"]):
                if dim_other != dim:
                    sys.stdout.write(" + "+config["dim_names"][dim_other]+"")

                    for dim_other_t in range(dim_other+1, config["num_dims"]):
                        if dim_other_t != dim:
                            sys.stdout.write("*block_size_"+config["dim_names"][dim_other_t])
            sys.stdout.write(";\n")

    sys.stdout.write("\n")
    sys.stdout.write(indent+"double center_val = center[index_center];\n")

    for dim in range(config["num_dims"]):
        for direction in range(0, 2):
            sys.stdout.write(indent+"double "+config["dimref_names"][dim][direction]+"_val = ")
            if direction == 0:
                sys.stdout.write("("+config["dim_names"][dim]+" == 0) ?\n"+indent+"\t("+config["dimref_names"][dim][direction]+" ? "+config["dimref_names"][dim][direction]+"[index_"+config["dimref_names"][dim][direction]+"] : 0.0) :\n"+indent+"\t(("+config["dim_names"][dim]+"_dir == 1) ?\n"+indent+"\t\t"+otherdim_prefix(dim, config["dim_names"])+"_saver["+otherdim_prefix(dim, config["dim_names"])+"_saver_index] :\n"+indent+"\t\tcenter[index_"+config["dimref_names"][dim][direction]+"_center])")
            else:
                sys.stdout.write("("+config["dim_names"][dim]+" == block_size_"+config["dim_names"][dim]+"-1) ?\n"+indent+"\t("+config["dimref_names"][dim][direction]+" ? "+config["dimref_names"][dim][direction]+"[index_"+config["dimref_names"][dim][direction]+"] : 0.0) :\n"+indent+"\t(("+config["dim_names"][dim]+"_dir == 1) ?\n"+indent+"\t\tcenter[index_"+config["dimref_names"][dim][direction]+"_center] :\n"+indent+"\t\t"+otherdim_prefix(dim, config["dim_names"])+"_saver["+otherdim_prefix(dim, config["dim_names"])+"_saver_index])")
            sys.stdout.write(";\n");

    sys.stdout.write("\n")

    sys.stdout.write(indent+"double new_val = (center_val")
    for dim in range(config["num_dims"]):
        for direction in range(0, 2):
            sys.stdout.write("+"+config["dimref_names"][dim][direction]+"_val")
    sys.stdout.write(") / "+str(2*config["num_dims"]+1)+".0;\n")

    sys.stdout.write("\n")

    sys.stdout.write(indent+"center[index_center] = new_val;\n")

    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write(indent+otherdim_prefix(dim, config["dim_names"])+"_saver["+otherdim_prefix(dim, config["dim_names"])+"_saver_index] = center_val;\n")

    for dim in range(config["num_dims"]):
        indent = indent[:len(indent)-1]
        sys.stdout.write(indent+"}\n");

    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        indent = "\t"
        for dim_other in range(config["num_dims"]):
            if dim_other != dim:
                sys.stdout.write(indent+"for(int "+config["dim_names"][dim_other]+" = 0; "+config["dim_names"][dim_other]+" < block_size_"+config["dim_names"][dim_other]+"; "+config["dim_names"][dim_other]+"++) {\n")
                indent = indent + "\t"

        sys.stdout.write(indent+"int "+otherdim_prefix(dim, config["dim_names"])+"_index = 0")

        for dim_other in range(config["num_dims"]):
            if dim_other != dim:
                sys.stdout.write(" + "+config["dim_names"][dim_other]);
                for dim_other_t in range(dim_other + 1, config["num_dims"]):
                    if dim_other_t != dim_other and dim_other_t != dim:
                        sys.stdout.write("*block_size_"+config["dim_names"][dim_other_t]);
        sys.stdout.write(";\n");

        for direction in range(2):
            sys.stdout.write("\n")
            sys.stdout.write(indent+"if("+config["dimref_names"][dim][direction]+") {\n");
            sys.stdout.write(indent+"\tint index_center_"+config["dimref_names"][dim][direction]+" = 0")
            for dim_other in range(config["num_dims"]):
                if dim_other == dim:
                    if direction == 0:
                        sys.stdout.write(" + 0")
                    else:
                        sys.stdout.write(" + (block_size_"+config["dim_names"][dim_other]+"-1)")
                else:
                    sys.stdout.write(" + "+config["dim_names"][dim_other]+"")

                for dim_other_t in range(dim_other+1, config["num_dims"]):
                    sys.stdout.write("*block_size_"+config["dim_names"][dim_other_t])
            sys.stdout.write(";\n")

            sys.stdout.write(indent+"\t"+config["dimref_names"][dim][direction]+"["+otherdim_prefix(dim, config["dim_names"])+"_index] = center[index_center_"+config["dimref_names"][dim][direction]+"];\n")
            sys.stdout.write(indent+"}\n");

        for dim_other in range(config["num_dims"]):
            if dim_other != dim:
                indent = indent[:len(indent)-1]
                sys.stdout.write(indent+"}\n");

        sys.stdout.write("\n");

    sys.stdout.write("}\n");

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
    sys.stdout.write("				       \"  -o <output file>             Write data to output file, default is stream_df_jacobi_%dd_reuse.out\\n\",\n"%config["num_dims"])
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
        sys.stdout.write("*N_"+config["dim_names"][dim])
    sys.stdout.write(";\n")

    sys.stdout.write("	double* matrix = malloc_interleaved(matrix_size);\n")

    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("	int blocks_"+config["dim_names"][dim]+" = N_"+config["dim_names"][dim]+" / block_size_"+config["dim_names"][dim]+";\n")

    sys.stdout.write("\n")

    sys.stdout.write("	int blocks = 1")
    for dim in range(config["num_dims"]):
        sys.stdout.write(" * blocks_"+config["dim_names"][dim])
    sys.stdout.write(";\n")

    sys.stdout.write("\n")

    sys.stdout.write("	int sdfbarrier[2] __attribute__((stream));\n")
    sys.stdout.write("	sdfbarrier_ref = malloc(2*sizeof (void *));\n")
    sys.stdout.write("	memcpy (sdfbarrier_ref, sdfbarrier, 2*sizeof (void *));\n")
    sys.stdout.write("\n")

    sys.stdout.write("	double scenter[(numiters+2)*blocks] __attribute__((stream));\n")
    sys.stdout.write("	scenter_ref = malloc((numiters+2)*blocks * sizeof (void *));\n")
    sys.stdout.write("	memcpy (scenter_ref, scenter, (numiters+2)*blocks * sizeof (void *));\n")
    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        for direction in range(0, 2):
            sys.stdout.write("	double s"+config["dimref_names"][dim][direction]+"[(numiters+2)*blocks] __attribute__((stream));\n")
            sys.stdout.write("	s"+config["dimref_names"][dim][direction]+"_ref = malloc((numiters+2)*blocks * sizeof (void *));\n")
            sys.stdout.write("	memcpy (s"+config["dimref_names"][dim][direction]+"_ref, s"+config["dimref_names"][dim][direction]+", (numiters+2)*blocks * sizeof (void *));\n")
            sys.stdout.write("\n")

    sys.stdout.write("	if(res_file == NULL)\n")
    sys.stdout.write("		res_file = fopen(\"stream_df_jacobi_%dd_reuse.out\", \"w\");\n"%config["num_dims"])
    sys.stdout.write("\n")

    indent = ""
    for dim in range(config["num_dims"]):
        indent = indent + "\t"
        sys.stdout.write(indent+"for(int "+config["dim_names"][dim]+" = 0; "+config["dim_names"][dim]+" < N_"+config["dim_names"][dim]+"; "+config["dim_names"][dim]+"++) {\n")

    indent = indent + "\t"

    sys.stdout.write(indent + "size_t pos = 0")

    for dim in range(config["num_dims"]):
        sys.stdout.write(" + "+config["dim_names"][dim])
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*N_"+config["dim_names"][dim_other])
    sys.stdout.write(";\n\n")

    sys.stdout.write(indent + "matrix[pos] = 0.0;\n\n")
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
    sys.stdout.write("	openstream_start_hardware_counters();\n")
    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        sys.stdout.write("	int tasks_per_block_"+config["dim_names"][dim]+" = int_min(blocks_"+config["dim_names"][dim]+", 4);\n")
    sys.stdout.write("\n")

    indent = ""
    for dim in range(config["num_dims"]):
        indent = indent + "\t"
        sys.stdout.write(indent+"for(int oid_"+config["dim_names"][dim]+" = 0; oid_"+config["dim_names"][dim]+" < blocks_"+config["dim_names"][dim]+"; oid_"+config["dim_names"][dim]+" += tasks_per_block_"+config["dim_names"][dim]+") {\n")

    indent = indent + "\t"

    sys.stdout.write(indent+"#pragma omp task\n")
    sys.stdout.write(indent+"{\n")
    indent = indent + "\t"

    for dim in range(config["num_dims"]):
        sys.stdout.write(indent+"for(int id_"+config["dim_names"][dim]+" = oid_"+config["dim_names"][dim]+"; id_"+config["dim_names"][dim]+" < oid_"+config["dim_names"][dim]+" + tasks_per_block_"+config["dim_names"][dim]+"; id_"+config["dim_names"][dim]+"++) {\n")
        indent = indent + "\t"

    # sys.stdout.write(indent+"\tfor(int oid_"+config["dim_names"][config["num_dims"]-1]+" = 0; oid_"+config["dim_names"][config["num_dims"]-1]+" < blocks_"+config["dim_names"][config["num_dims"]-1]+"; oid_"+config["dim_names"][config["num_dims"]-1]+" += tasks_per_block) {\n")
    # sys.stdout.write(indent+"\t\tfor(int id_"+config["dim_names"][config["num_dims"]-1]+" = oid_"+config["dim_names"][config["num_dims"]-1]+"; id_"+config["dim_names"][config["num_dims"]-1]+" < oid_"+config["dim_names"][config["num_dims"]-1]+" + tasks_per_block; id_"+config["dim_names"][config["num_dims"]-1]+"++) {\n")

    sys.stdout.write(indent+"create_initial_task(matrix, numiters")

    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim]);

    for dim in range(config["num_dims"]):
        sys.stdout.write(", block_size_"+config["dim_names"][dim]);

    for dim in range(config["num_dims"]):
        sys.stdout.write(", id_"+config["dim_names"][dim]);

    sys.stdout.write(");\n")

    sys.stdout.write(indent+"create_next_iteration_task(matrix, numiters, 0")
    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim]);

    for dim in range(config["num_dims"]):
        sys.stdout.write(", block_size_"+config["dim_names"][dim]);

    for dim in range(config["num_dims"]):
        sys.stdout.write(", id_"+config["dim_names"][dim]);

    sys.stdout.write(");\n")

    for dim in range(config["num_dims"]):
        indent = indent[:len(indent)-1]
        sys.stdout.write(indent+"}\n")

    indent = indent[:len(indent)-1]
    sys.stdout.write(indent+"}\n")

    for dim in range(config["num_dims"]):
        indent = indent[:len(indent)-1]
        sys.stdout.write(indent+"}\n");

    sys.stdout.write("\n")
    sys.stdout.write("	/* Wait for all the tasks to finish */\n")
    sys.stdout.write("	int dfbarrier_tokens[blocks];\n")
    sys.stdout.write("	#pragma omp task input(sdfbarrier_ref[0] >> dfbarrier_tokens[blocks])\n")
    sys.stdout.write("	{\n")
    if config["debug_printf"]:
        sys.stdout.write("		printf(\"FINISH\\n\");\n")
    sys.stdout.write("	}\n")
    sys.stdout.write("\n")
    sys.stdout.write("	#pragma omp taskwait\n")
    sys.stdout.write("\n")
    sys.stdout.write("	openstream_pause_hardware_counters();\n")
    sys.stdout.write("	gettimeofday(&end, NULL);\n")
    sys.stdout.write("\n")
    sys.stdout.write("	printf(\"%.5f\\n\", tdiff(&end, &start));\n")
    sys.stdout.write("\n")
    sys.stdout.write("	#if _WITH_OUTPUT\n")

    sys.stdout.write("	dump_matrix_%dd(matrix, res_file"%config["num_dims"])
    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim])
    sys.stdout.write(");\n")

    sys.stdout.write("	#endif\n")
    sys.stdout.write("\n")
    sys.stdout.write("	#if _WITH_BINARY_OUTPUT\n")
    sys.stdout.write("	dump_matrix_binary(res_file, N, matrix);\n")
    sys.stdout.write("	#endif\n")
    sys.stdout.write("\n")
    sys.stdout.write("	fclose(res_file);\n")
    sys.stdout.write("\n")

    sys.stdout.write("	free(matrix);\n")
    sys.stdout.write("	free(scenter_ref);\n")
    sys.stdout.write("	free(sdfbarrier_ref);\n")

    for dim in range(config["num_dims"]):
        for direction in range(0, 2):
            sys.stdout.write("	free(s"+config["dimref_names"][dim][direction]+"_ref);\n")

    sys.stdout.write("\n")
    sys.stdout.write("	return 0;\n")
    sys.stdout.write("}\n")
    sys.stdout.write("\n")

def dump_file(config):
    dump_global_defs(config)
    sys.stdout.write("\n");

    dump_jacobi_fun_signature(config)
    sys.stdout.write(";\n");

    dump_init_fun_signature(config)
    sys.stdout.write(";\n");

    dump_finish_fun_signature(config)
    sys.stdout.write(";\n");

    dump_create_init_followup_task_fun_signature(config)
    sys.stdout.write(";\n");

    dump_create_followup_task_fun_signature(config)
    sys.stdout.write(";\n");

    dump_create_initial_task_fun_signature(config)
    sys.stdout.write(";\n");

    dump_create_next_iteration_task_fun_signature(config)
    sys.stdout.write(";\n");

    dump_create_terminal_task_fun_signature(config)
    sys.stdout.write(";\n");

    dump_dump_matrix_fun_signature(config)
    sys.stdout.write(";\n\n");

    dump_dump_matrix_fun(config)
    sys.stdout.write("\n")

    dump_jacobi_fun(config)
    sys.stdout.write("\n")

    dump_init_fun(config)
    sys.stdout.write("\n")

    dump_finish_fun(config)
    sys.stdout.write("\n")

    dump_create_terminal_task_fun(config)
    sys.stdout.write("\n")

    dump_create_init_followup_task_fun(config)
    sys.stdout.write("\n")

    dump_create_followup_task_fun(config)
    sys.stdout.write("\n")

    dump_create_initial_task_fun(config)
    sys.stdout.write("\n")

    dump_create_next_iteration_task_fun(config)
    sys.stdout.write("\n")

    dump_main_fun(config)
    sys.stdout.write("\n")
