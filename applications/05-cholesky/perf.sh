#!/bin/bash

pkexec perf record -e cycles -e sched:sched_switch --switch-events --sample-cpu --call-graph dwarf -a -g -T -F 99 sh -c ' for ((n=0; n <100;n++)); do ./a.out ; done '
pkexec chown jabcross:jabcross perf.data
pkexec chmod ugo+rw perf.data
