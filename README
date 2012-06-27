** Data-flow runtime restrictions **

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


** Known issues **

1. Streams of arrays currently experience an ICE.

2. The OpenMP shared clause may not work in most cases.  Use a
firstprivate clause on a pointer instead.

