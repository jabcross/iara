perf record -e cycles -e sched:sched_switch --switch-events --sample-cpu --call-graph dwarf -a -g -T -F 99 $1 && chown $USER perf.data && chmod ugo+rw perf.data
