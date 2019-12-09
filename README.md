# OpenStream #

This project provides the compiler and runtime of OpenStream.

OpenStream is a stream programming language, designed as an incremental
extension to the OpenMP parallel programming language, and allowing to
express arbitrary dependence patterns between tasks in the form of task-level
data flow dependences. Programmers expose task parallelism while providing
the compiler with data flow information, through compiler annotations
(pragmas), used to generate code that dynamically builds a streaming program.
It allows to exploit task, pipeline and data parallelism.

Typical targets: GNU/Linux system on amd64 or aarch64.

## Using OpenStream ##

### Build Dependencies ###

#### On Debian / Ubuntu

```bash
sudo apt install gcc g++ gfortran flex bison cmake make autoconf automake pkg-config wget
```

#### Fedora / CentOS / Red Hat

```bash
sudo dnf install gcc gcc-c++ gcc-gfortran flex bison cmake make autoconf automake pkgconf wget
```

#### OpenSUSE

```bash
sudo zypper install gcc gcc-c++ gcc-gfortran flex bison cmake make autoconf automake pkgconf wget
```

#### Arch Linux

```bash
sudo pacman -S gcc gcc-fortran flex bison cmake make autoconf automake pkgconf wget
```

### Building OpenStream ###

```bash
make -j $(nproc)
```


## Data-flow runtime restrictions ##

Set of restrictions on the streaming model deriving from the use of a
DF runtime target.

1. Matching rules for burst/horizon values of each stream access
operation:

   - for any given stream, the input horizon is an integer multiple of
     the output burst of all tasks on the same stream

   - for any output clause, the horizon is always equal to the burst
     (no "poke" operation allowed)

   - for any input clause, the burst is either null (peek operation)
     or equal to the horizon.

2. All data produced on a stream must be consumed.  The program will
indefinitely wait if more data is produced than consumed, which is due
to the fact that a producer cannot execute as long as it doesn't know
its consumers (it writes directly in their DF frame).  It is possible
to activate the option to discard unused data in the runtime
(semantics of the "tick" operation) by providing fake frames where
producers write the data to discard (necessary if some other stream's
data is used or if the task has side-effects).


## Known issues ##

1. Streams of arrays currently experience an ICE.

2. The OpenMP shared clause may not work in most cases.  Use a
firstprivate clause on a pointer instead.

