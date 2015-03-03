# Aurix Tracing
Aurix TriCore time tracing deamon for the DAP miniWiggler V3 based on mcdxdas.dll.

# Introduction

The tracing deamon leverages on a library, provided by Infineon, called `mcdxdas.dll`. Such library, that has to be installed in the host running the tracing deamon, allows
to exploit the memory inspection capabilities provided by the DAP
miniWiggler. 

# Usage

By default, the tracing deamon is configured to poll each
core data scratchpad at the `0xD0000000` location every 1 millisecond.
Trace files are saved by default in the same directory where the deamon
is run and are called `trace_coreX.txt`.

The tracing program accepts the following options:

-   `–buffer-address, -a`: allows to manually specify the address of the
    trace buffer

-   `–polling-period, -p`: allows to set the polling period, in
    microseconds

-   `–output, -o`: allows to set a prefix (`<prefix>`) for the trace
    files that will be called `<prefix>X.txt`

-   `–singlecore, -c`: enables tracing only for the master core
