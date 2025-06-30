#!/bin/bash
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

if [[ $INSTANCE_DIR == "" ]]; then
  echo "must pass in INSTANCE_DIR"
  exit 1
fi

mkdir -p $INSTANCE_DIR/profile
cd $INSTANCE_DIR/profile

# check correctness

$INSTANCE_DIR/build/a.out --write-result --no-scheduler >result.noscheduler && $INSTANCE_DIR/build/a.out --write-result >result.scheduler && diff result.noscheduler result.scheduler

if [[ $? != 0 ]]; then
  echo Incorrect result
  exit 1
fi

# Run timing 10 times and extract wall time and max resident size
echo "run,wall_time_seconds,max_resident_size_kb" >timing_results.csv

for i in {1..10}; do
  echo "Running iteration $i..."
  \time -v $INSTANCE_DIR/build/a.out >time_out_$i.txt 2>time_err_$i.txt

  # Extract wall time (convert to seconds in decimal format)
  wall_time=$(grep "Elapsed (wall clock) time" time_err_$i.txt | sed 's/.*: //' | awk -F: '
    NF==2 { printf "%.6f", $1*60 + $2 }  # m:ss.ss format
    NF==1 { printf "%.6f", $1 }          # ss.ss format
    NF==3 { printf "%.6f", $1*3600 + $2*60 + $3 }  # h:mm:ss format
  ')

  # Extract max resident size (in kbytes)
  max_resident=$(grep "Maximum resident set size" time_err_$i.txt | grep -o '[0-9]*')

  echo "$i,$wall_time,$max_resident" >>timing_results.csv
done

# Create summary statistics
echo "Creating summary statistics..."
python3 $SCRIPT_DIR/generate_timing_stats.py timing_results.csv timing_summary.txt

cat timing_summary.txt
