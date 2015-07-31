#!/usr/bin/python

import sys
from seidel_positions import *

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
            if(config["dim_configs"][dim] == seidel_positions.first):
                sys.stdout.write(" && id_"+config["dim_names"][dim]+" == 0 && blocks_"+config["dim_names"][dim]+" > 1")
            elif(config["dim_configs"][dim] == seidel_positions.mid):
                sys.stdout.write(" && id_"+config["dim_names"][dim]+" > 0 && id_"+config["dim_names"][dim]+" < blocks_"+config["dim_names"][dim]+"-1")
            elif(config["dim_configs"][dim] == seidel_positions.last):
                sys.stdout.write(" && id_"+config["dim_names"][dim]+" == blocks_"+config["dim_names"][dim]+"-1 && blocks_"+config["dim_names"][dim]+" > 1")
            elif(config["dim_configs"][dim] == seidel_positions.span):
                sys.stdout.write(" && blocks_"+config["dim_names"][dim]+" == 1")
        sys.stdout.write(") {\n")
        dump_init_task(config, "\t\t")
        sys.stdout.write("\t}\n")

    sys.stdout.write("}\n")

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
        if config["dim_configs"][dim] != seidel_positions.first and config["dim_configs"][dim] != seidel_positions.span:
            sys.stdout.write(", \\\n"+pref+"\t\t")
            sys.stdout.write("s"+config["dimref_names"][dim][0]+"_ref[id] << "+config["dimref_names"][dim][0]+"_out["+config["dimref_names"][dim][0]+"_out_size]")
            num_clauses = num_clauses + 1

    sys.stdout.write(")\n")
    sys.stdout.write(pref+"{\n")
    sys.stdout.write(pref+"\tseidel_%dd_df_init(matrix"%config["num_dims"])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", id_"+config["dim_names"][dim])

    sys.stdout.write(", center_out")

    for dim in range(config["num_dims"]):
        if config["dim_configs"][dim] != seidel_positions.first and config["dim_configs"][dim] != seidel_positions.span:
            sys.stdout.write(", "+config["dimref_names"][dim][0]+"_out")
        else:
            sys.stdout.write(", NULL")

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

def dump_init_fun_signature(config):
    sys.stdout.write("void seidel_%dd_df_init(double* matrix"%config["num_dims"])

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

def dump_finish_fun_signature(config):
    sys.stdout.write("void seidel_%dd_df_finish(double* matrix"%config["num_dims"])

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
