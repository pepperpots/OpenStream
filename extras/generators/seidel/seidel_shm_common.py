#!/usr/bin/python

import sys
from seidel_positions import *
import seidel_common
from functools import reduce

def ilen(iterable):
    return reduce(lambda sum, element: sum + 1, iterable, 0)

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
    sys.stdout.write("scenter_ref[id] >> center_in_token[1]")

    for dim in range(config["num_dims"]):
        if config["dim_configs"][dim] != seidel_positions.first and config["dim_configs"][dim] != seidel_positions.span:
            sys.stdout.write(", \\\n"+pref+"\t\t")
            sys.stdout.write("s"+config["dimref_names"][dim][0]+"_ref[id] >> "+config["dimref_names"][dim][0]+"_in_token[1]")
            num_clauses = num_clauses + 1

    sys.stdout.write(") \\\n")
    sys.stdout.write(pref+"\toutput(sdfbarrier_ref[0] << dfbarrier_token[1])\n")
    sys.stdout.write(pref+"{\n")
    sys.stdout.write(pref+"}\n")

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

    sys.stdout.write("\tint dfbarrier_token[1];\n")

    sz = "1"
    for dim in range(config["num_dims"]):
        sz = sz + "*block_size_"+config["dim_names"][dim]

    sys.stdout.write("\tint center_in_token[1];\n")

    for dim in range(config["num_dims"]):
        sz = "1"

        for dim_other in range(config["num_dims"]):
            if dim_other != dim:
                sz = sz + "*block_size_"+config["dim_names"][dim_other]

        sys.stdout.write("\tint "+config["dimref_names"][dim][0]+"_in_token[1];\n")
        sys.stdout.write("\tint "+config["dimref_names"][dim][1]+"_in_token[1];\n")

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
        dump_terminal_task(config, "\t\t")
        sys.stdout.write("\t}\n")

    sys.stdout.write("}\n")

def dump_seidel_fun_signature(config):
    sys.stdout.write("void seidel_%dd(double* matrix"%config["num_dims"])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int N_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int block_size_"+config["dim_names"][dim])

    for dim in range(config["num_dims"]):
        sys.stdout.write(", int id_"+config["dim_names"][dim])

    sys.stdout.write(")");

def dump_seidel_fun_loopnest(config, indent, mode, bounds):
    for d, bound in zip(config["dim_names"], bounds):
        sys.stdout.write(indent+"for(int "+d+" = "+bound[0]+"; "+d+" < "+bound[1]+"; "+d+"++) {\n")
        indent = indent+"\t"

    if mode == "main":
        center_expr = "0"
        for dim_other in range(config["num_dims"]):
            center_expr += " + (id_"+config["dim_names"][dim_other]+"*block_size_"+config["dim_names"][dim_other]+" + "+config["dim_names"][dim_other]+")"

            for dim_other_t in range(dim_other+1, config["num_dims"]):
                center_expr += "*N_"+config["dim_names"][dim_other_t]

        sys.stdout.write(indent + "matrix["+center_expr+"] = (matrix["+center_expr+"]")

        for dim in range(config["num_dims"]):
            for direction in range(0, 2):
                sys.stdout.write(" +\n"+indent+"\tmatrix[0")
                for dim_other in range(config["num_dims"]):
                    if dim_other == dim:
                        if direction == 0:
                            sys.stdout.write(" + (id_"+config["dim_names"][dim_other]+"*block_size_"+config["dim_names"][dim_other]+" + "+config["dim_names"][dim_other]+" - 1)")
                        else:
                            sys.stdout.write(" + (id_"+config["dim_names"][dim_other]+"*block_size_"+config["dim_names"][dim_other]+" + "+config["dim_names"][dim_other]+" + 1)")
                    else:
                        sys.stdout.write(" + (id_"+config["dim_names"][dim_other]+"*block_size_"+config["dim_names"][dim_other]+" + "+config["dim_names"][dim_other]+")")

                    for dim_other_t in range(dim_other+1, config["num_dims"]):
                        sys.stdout.write("*N_"+config["dim_names"][dim_other_t])
                sys.stdout.write("]")

        sys.stdout.write(") / "+str(2*config["num_dims"]+1)+".0;\n")
    else:
        for dim in range(config["num_dims"]):
            sys.stdout.write(indent+"int global_"+config["dim_names"][dim]+" = id_"+config["dim_names"][dim]+" * block_size_"+config["dim_names"][dim]+" + "+config["dim_names"][dim]+";\n")

        sys.stdout.write("\n")

        for dim in range(config["num_dims"]):
            for direction in range(0, 2):
                sys.stdout.write(indent+"int global_index_"+config["dimref_names"][dim][direction]+" = 0")
                for dim_other in range(config["num_dims"]):
                    if dim_other == dim:
                        if direction == 0:
                            sys.stdout.write(" + (global_"+config["dim_names"][dim_other]+"-1)")
                        else:
                            sys.stdout.write(" + (global_"+config["dim_names"][dim_other]+"+1)")
                    else:
                        sys.stdout.write(" + global_"+config["dim_names"][dim_other]+"")

                    for dim_other_t in range(dim_other+1, config["num_dims"]):
                        sys.stdout.write("*N_"+config["dim_names"][dim_other_t])
                sys.stdout.write(";\n")

        sys.stdout.write("\n")

        sys.stdout.write(indent+"int global_index_center = 0")
        for dim in range(config["num_dims"]):
            sys.stdout.write(" + global_"+config["dim_names"][dim]+"")
            for dim_other in range(dim+1, config["num_dims"]):
                sys.stdout.write("*N_"+config["dim_names"][dim_other])
        sys.stdout.write(";\n\n")

        sys.stdout.write(indent+"double center_val = matrix[global_index_center];\n")

        for dim in range(config["num_dims"]):
            for direction in range(0, 2):
                sys.stdout.write(indent+"double "+config["dimref_names"][dim][direction]+"_val = ")
                if direction == 0:
                    sys.stdout.write("(id_"+config["dim_names"][dim]+" == 0 && "+config["dim_names"][dim]+" == 0) ? 0.0 : matrix[global_index_"+config["dimref_names"][dim][direction]+"];")
                else:
                    sys.stdout.write("(id_"+config["dim_names"][dim]+" == blocks_"+config["dim_names"][dim]+"-1 && "+config["dim_names"][dim]+" == block_size_"+config["dim_names"][dim]+"-1) ? 0.0 : matrix[global_index_"+config["dimref_names"][dim][direction]+"];")
                sys.stdout.write("\n");

        sys.stdout.write("\n")

        sys.stdout.write(indent+"matrix[global_index_center] = (center_val")
        for dim in range(config["num_dims"]):
            for direction in range(0, 2):
                sys.stdout.write("+"+config["dimref_names"][dim][direction]+"_val")
        sys.stdout.write(") / "+str(2*config["num_dims"]+1)+".0;\n")

    for dim in range(config["num_dims"]):
        indent = indent[:len(indent)-1]
        sys.stdout.write(indent+"}\n");

    sys.stdout.write("\n")

def dump_seidel_fun(config):
    dump_seidel_fun_signature(config)
    sys.stdout.write("\n{\n");

    indent = "\t"

    for dim in range(config["num_dims"]):
        sys.stdout.write("\tint blocks_"+config["dim_names"][dim]+" = N_"+config["dim_names"][dim]+"/block_size_"+config["dim_names"][dim]+";\n")
    sys.stdout.write("\n")

    sys.stdout.write("\tint id = 0")

    for dim in range(config["num_dims"]):
        sys.stdout.write(" + id_"+config["dim_names"][dim]);
        for dim_other in range(dim+1, config["num_dims"]):
            sys.stdout.write("*blocks_"+config["dim_names"][dim_other])
    sys.stdout.write(";\n")
    sys.stdout.write("\n")

    for dim in range(config["num_dims"]):
        for direction in range(2):
            sys.stdout.write("\tint id_"+config["dimref_names"][dim][direction]+" = 0")

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

    sys.stdout.write("\n")

    for mode in ["intro", "main", "outro" ]:
        if mode == "intro":
            sys.stdout.write(indent+"/* Intro: "+("-".join(config["dim_names"]))+" corner */\n");
            bounds = map(lambda x: ["0", "1"], config["dim_names"])
            dump_seidel_fun_loopnest(config, indent, mode, bounds)

            for nzerodims in range(config["num_dims"]-1, 0, -1):
                for comb in seidel_common.partition_left_right(config["dim_names"], nzerodims):
                    zerodims = comb[0]
                    freedims = comb[1]

                    sys.stdout.write(indent+"/* Intro: "+("-".join(freedims))+" "+seidel_common.geom_name(ilen(freedims))+" */\n");
                    bounds = map(lambda x: x in zerodims and ["0", "1"] or ["1", "block_size_"+x], config["dim_names"])
                    dump_seidel_fun_loopnest(config, indent, mode, bounds)
        elif mode == "main":
            sys.stdout.write(indent+"/* Main part */\n");
            bounds = map(lambda x: ["1", "block_size_"+x+"-1"], config["dim_names"])
            dump_seidel_fun_loopnest(config, indent, mode, bounds)
        elif mode == "outro":
            for nzerodims in range(1, config["num_dims"]):
                for comb in seidel_common.partition_left_right(config["dim_names"], nzerodims):
                    zerodims = comb[0]
                    freedims = comb[1]

                    sys.stdout.write(indent+"/* Outro: "+("-".join(freedims))+" "+seidel_common.geom_name(ilen(freedims))+" */\n");
                    bounds = map(lambda x: x in zerodims and ["block_size_"+x+"-1", "block_size_"+x] or ["1", "block_size_"+x+"-1"], config["dim_names"])
                    dump_seidel_fun_loopnest(config, indent, mode, bounds)

            sys.stdout.write(indent+"/* Intro: "+("-".join(config["dim_names"]))+" corner */\n");
            bounds = map(lambda x: ["block_size_"+x+"-1", "block_size_"+x], config["dim_names"])
            dump_seidel_fun_loopnest(config, indent, mode, bounds)

    sys.stdout.write("}\n");
