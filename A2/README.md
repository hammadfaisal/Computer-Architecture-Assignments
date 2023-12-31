# Pipelined MIPS Processor Simulator
This is a simulator for a pipelined MIPS processor. The simulator is written in C++. It supports 5-stage and 7-9 stage pipelining. The simulator also supports bypassing and forwarding. The simulator can be used to run MIPS assembly programs and view the contents of the registers and memory at each cycle.

## Usage
Run the following commands to compile and run the sample program:
```bash
$ make compile
```
this compiles all the files and creates respective executeables. To run simulator-
```bash
$ make run_5stage
$ make run_5stage_bypass
$ make run_79stage
$ make run_79stage_bypass
```
