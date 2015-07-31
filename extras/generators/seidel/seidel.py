#!/usr/bin/python

import sys
import stream_df_seidel
import seidel_seq
import stream_df_seidel_reuse
import stream_seidel_from_df
import stream_seidel_seqctrl
import seidel_positions

from optparse import OptionParser

configs = [
    #1D
    {"dim_names" : ["x"],
     "dimref_names" : [["left", "right"]],
     "init_positions" : [{"pos" : ["1"], "val" : "500"},
                         {"pos" : ["N_x-1"], "val" : "500"}]},

    #2D
    {"dim_names" : ["y", "x"],
     "dimref_names" : [["top", "bottom"],
                       ["left", "right"]],
     "init_positions" : [{"pos" : ["0", "0"], "val" : "500"},
                         {"pos" : ["N_y-1", "N_x-1"], "val" : "500"}]},

    #3D
    {"dim_names" : ["z", "y", "x"],
     "dimref_names" : [["back", "front"],
                       ["top", "bottom"],
                       ["left", "right"]],
     "init_positions" : [{"pos" : ["1", "1", "1"], "val" : "500"},
                         {"pos" : ["N_z-1", "N_y-1", "N_x-1"], "val" : "500"}]}]

###########
config = {"num_dims" : None,
          "dim_names" : None,
          "dimref_names" : None,
          "init_positions" : None,
          "debug_printf" : False}

parser = OptionParser()
parser.add_option("-v", "--version", dest="version", type="string", help="Version (e.g., seq, stream_df, stream_df_reuse, stream_from_df)")
parser.add_option("-d", "--dimensions", dest="dimensions", type="int", help="Number of dimensions")
(options, args) = parser.parse_args()

if options.dimensions == None:
    sys.stderr.write("Please specify the number of dimensions.\n")
    sys.exit(1)

if options.version == None:
    sys.stderr.write("Please specify the version to generate.\n")
    sys.exit(1)

if len(configs) < options.dimensions:
    sys.stderr.write("There is no configuration for a %dd version available.\n"%options.dimensions)
    sys.exit(1)

config["num_dims"] = options.dimensions
config["dim_names"] = configs[options.dimensions-1]["dim_names"]
config["dimref_names"] = configs[options.dimensions-1]["dimref_names"]
config["init_positions"] = configs[options.dimensions-1]["init_positions"]

if options.version == "stream_df":
    stream_df_seidel.dump_file(config)
elif options.version == "stream_df_reuse":
    stream_df_seidel_reuse.dump_file(config)
elif options.version == "stream_from_df":
    stream_seidel_from_df.dump_file(config)
elif options.version == "stream_seqctrl":
    stream_seidel_seqctrl.dump_file(config)
elif options.version == "seq":
    seidel_seq.dump_file(config)
else:
    sys.stderr.write("Unknown version \""+options.version+"\".\n")
    sys.exit(1)
