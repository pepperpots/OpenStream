#!/usr/bin/env bash
## This script generates machine-specific configuration files, for a
## given run configuration.

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


function do_one_run_set ()
{
    BC_ARGS=""
    for i in $(seq $tuning_runs)
    do
	echo -n "."

	# Execute the benchmark
	TM=$($1)

	# Check if run was ok
	if [ $? -ne 0 ]
	then
	    echo >&2
	    echo "ERROR: Failed to execute $1." >&2
	    #cat log >&2
	    echo >&2
	    exit 1
	fi

	# Build bc command (time of run 1 + time of run 2 + ...)
	BC_ARGS="$BC_ARGS$TM+"
    done
    echo -n "] Done. "

    BC_ARGS="($BC_ARGS 0)/$tuning_runs"

    # Calculate average run time in seconds
    AVG=$(echo "scale=6;$BC_ARGS" | bc)

    # Convert to microseconds
    AVG_US=$(echo "scale=6;$AVG*1000000" | bc | cut -f1 -d".")
    echo "average: $AVG seconds"

    # Update minimal execution time if necessary
    if [ -z "$MIN_AVGTIME_US" ] || [ $AVG_US -lt $MIN_AVGTIME_US ]
    then
	MIN_AVGTIME_US=$AVG_US
	MIN_AVGTIME_S=$AVG
	select_command_line=$1
    fi
}


## Tune one specific target for a given benchmark.  Takes 3
## parameters, the benchmark name (its directory inside examples/),
## the current target (a version of the implementation of the
## benchmark) and the configuration file where the results will be
## dumped.
function do_one_target ()
{
    # Check if target executable is available
    if [ ! -f "$2" ]
    then
	echo "ERROR: Cannot find target \"$1/$2\". Did you forget to build it?" >&2
	exit 1
    fi

    params_="conf_${ARG_CONF}_$1_params"
    params=${!params_}

    param1_="conf_${ARG_CONF}_$1_tunable_param1"
    param1=${!param1_}
    param1_range_="conf_${ARG_CONF}_$1_tunable_param1_range"
    param1_range=${!param1_range_}
    param2_="conf_${ARG_CONF}_$1_tunable_param2"
    param2=${!param2_}
    param2_range_="conf_${ARG_CONF}_$1_tunable_param2_range"
    param2_range=${!param2_range_}

    # Check that we have tunable parameters, otherwise just use the
    # base parameters and return.
    select_command_line="./$2 $params"
    if [ -z "$param1" -o -z "$param1_range" ]
    then
	echo "run_$2_command=\"$select_command_line\"" >> $3
	return
    fi


# Minimal execution time in microseconds
# (avoids floating point handling in the loop)
    MIN_AVGTIME_US=""
# Minimal execution time in seconds
    MIN_AVGTIME_S=""

    echo ""
    echo "Tuning runs on $1/$2. Scheduled for $tuning_runs executions for each parameter set..."

    for param1_value in $(seq $param1_range | xargs)
    do
	if [ ! -z "$param2" -a ! -z "$param2_range" ]
	then
	    for param2_value in $(seq $param2_range__ | xargs)
	    do
		echo -n "* Executing $2 $params $param1$param1_value $param2$param2_value ["
		do_one_run_set "./$2 $params $param1$param1_value $param2$param2_value"
	    done
	else
	    echo -n "* Executing $2 $params $param1$param1_value ["
	    do_one_run_set "./$2 $params $param1$param1_value"
	fi
    done

    echo "run_$2_command=\"$select_command_line\"" >> $3
    echo "Selected configuration is: \"$select_command_line\" executing on average in $MIN_AVGTIME_S seconds."
}

## Tune one benchmark.  Takes 2 parameters, the benchmark name (its
## directory inside examples/) and the configuration file where the
## results will be dumped.
function tune_bench ()
{
    pushd $1

    targets_="conf_${ARG_CONF}_$1_targets"
    targets=${!targets_}

    for target in $targets
    do
	do_one_target $1 $target $2
    done

    popd
}


## Check for pre-existing configuration directories and files.  If all
## files are already generated do not re-run the tuning phase. REMOVE
## configuration files to force a new tuning run for a given machine.
machine_config_dir=$PWD/`uname -n`_config
if [ ! -d $machine_config_dir ]
then
    mkdir $machine_config_dir
fi
if [ ! -f $machine_config_dir/system_config ]
then
    echo "-------------------------------------------------------------------------" >> $machine_config_dir/system_config
    uname -a >> $machine_config_dir/system_config
    echo "-------------------------------------------------------------------------" >> $machine_config_dir/system_config
    cat /proc/cpuinfo >> $machine_config_dir/system_config
    echo "-------------------------------------------------------------------------" >> $machine_config_dir/system_config
    cat /proc/meminfo >> $machine_config_dir/system_config
fi

this_conf_dir=$machine_config_dir/conf_$ARG_CONF
if [ ! -d $this_conf_dir ]
then
    mkdir $this_conf_dir
fi

benchmarks_="conf_${ARG_CONF}_benchmarks"
benchmarks=${!benchmarks_}
for bench in ${benchmarks}
do
    bench_config_file=$this_conf_dir/conf_${ARG_CONF}_$bench.config

    if [ ! -f $bench_config_file ]
    then
	tune_bench $bench $bench_config_file
    fi
done



