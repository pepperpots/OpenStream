#!/bin/sh

base_dir=./
rundir=$base_dir/runs
makedir () {
    DIRNAME=$1
    mkdir -p $DIRNAME
    if [ $? -ne 0 ]; then
	echo "ERROR: could not make directory $DIRNAME"
	exit 1
    fi
}
now () {
    echo `TZ=UTC date +"%Y_%m_%d_%H_%M_%S"`
}
[ -d $rundir ] || makedir $rundir
this_run=$rundir/run_`now`
makedir $this_run

RUNDIR=$this_run
EXEDIR=${PWD}/../
INPDIR=${PWD}/inputs
# RUNCMD="ccc_mprun -p standard -n 8 -N 4 -x "
RUNCMD=

#cd ${EXEDIR} && make && cd -

cd ${RUNDIR}
${RUNCMD} ${EXEDIR}/$1 -i ${INPDIR}/input_250x125_corner.nml 

#EOF
