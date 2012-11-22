#!/usr/bin/env bash
## This script completes a benchmark run for a given configuration..

source ./run.config

function print_usage_and_die()
{
    echo "Usage: $0 [option]"
    echo
    echo "Options:"
    echo "  -c <conf_name>           Name of run configuration to tune for"
    exit 1
}

# Configuration we should tune for
ARG_CONF=""
while getopts c:h opt
do
    case "$opt" in
	c) ARG_CONF="$OPTARG";;
	[?]|h) print_usage_and_die;;
    esac
done
shift $(($OPTIND-1))

# Check for mising argument
if [ -z "$ARG_CONF" ]
then
    echo "Please specify a configuration name with -c" >&2
    exit 1
fi


function do_runs ()
{
    echo -n "Executing $benchmark_runs runs for \"$1\": ["
    for i in $(seq $benchmark_runs)
    do
	echo -n "."
	$1 >> $2

	# Check if run was ok
	if [ $? -ne 0 ]
	then
	    echo >&2
	    echo "ERROR: Failed to execute $1." >&2
	    #cat log >&2
	    echo >&2
	    exit 1
	fi
    done
    echo "] Done. "
}

## Tune one benchmark.  Takes 2 parameters, the benchmark name (its
## directory inside examples/) and the configuration file where the
## results will be dumped.
function run_bench ()
{
    pushd $1

    targets_="conf_${ARG_CONF}_$1_targets"
    targets=${!targets_}

    for target in $targets
    do
	command_="run_${target}_command"
	command=${!command_}
	this_log_file=$2/$(echo "$command" | sed 's/ /_/g').log

	do_runs "$command" $this_log_file
    done

    reference_="conf_${ARG_CONF}_$1_reference"
    reference=${!reference_}
    params_="conf_${ARG_CONF}_$1_params"
    params=${!params_}
    command="./$reference $params"
    this_log_file=$2/$(echo "$command" | sed 's/ /_/g').log

    do_runs "$command" $this_log_file

    popd
}


## Check for pre-existing configuration directories and files.  If all
## files are already generated do not re-run the tuning phase. REMOVE
## configuration files to force a new tuning run for a given machine.
machine_config_dir=$PWD/`uname -n`_config
if [ ! -d $machine_config_dir ]
then
    echo "ERROR: no machine configuration directory found. Run \"make clean-tuning\" at toplevel then retry."
    echo "This script should not be run independently, use \"make run-<configuration-name>\" at toplevel."
    exit 1
fi

this_conf_dir=$machine_config_dir/conf_$ARG_CONF
if [ ! -d $this_conf_dir ]
then
    echo "ERROR: no tuning configuration directory found. Run \"make clean-tuning\" at toplevel then retry."
    echo "This script should not be run independently, use \"make run-<configuration-name>\" at toplevel."
    exit 1
fi

## Make a directory to dump results
if [ -z $merge_logs_to_directory ]
then
    this_log_dir=$this_conf_dir/logs/`TZ=UTC date +"%Y_%m_%d_%H_%M_%S"`
else
    this_log_dir=$this_conf_dir/logs/$merge_logs_to_directory
fi
mkdir -p $this_log_dir

benchmarks_="conf_${ARG_CONF}_benchmarks"
benchmarks=${!benchmarks_}
for bench in ${benchmarks}
do
    bench_config_file=$this_conf_dir/conf_${ARG_CONF}_$bench.config
    if [ ! -f $bench_config_file ]
    then
	echo "ERROR: no configuration files found for benchmark $bench. Run \"make clean-tuning\" at toplevel then retry."
	echo "This script should not be run independently, use \"make run-<configuration-name>\" at toplevel."
	exit 1
    fi

    source $bench_config_file

    bench_log_dir=$this_log_dir/$bench
    mkdir -p $bench_log_dir

    run_bench $bench $bench_log_dir
done



