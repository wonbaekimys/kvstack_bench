#!/usr/bin/env bash

set -e
set -o pipefail

if [ $# -ne 2 ]; then
  echo "Usage: $0 <server_address:port> <num_clients>"
  exit 1
fi
server_address=$1
num_clients=$2
server_host=$(echo $server_address | cut -d ":" -f 1)
server_port=$(echo $server_address | cut -d ":" -f 2)
server_exec=$(realpath simplekv_server)
client_exec=$(realpath kvstack_bench.py)

python3 kvstack_init_lock.py $server_address

value_size=100
num_requests=1000
echo "========================================="
echo "Benchmark configuration:"
echo "  value_size: $value_size"
echo "  num_requests (per client): $num_requests"
echo "  num_clients: $num_clients"
echo "  toal requests: $((num_clients*num_requests))"
echo "-----------------------------------------"
#################################################
sleep 1
echo "*** Warming Up ***"
push_ratio=1
for ((i=0;i<num_clients;i++)); do
  python3 kvstack_bench.py $server_address $value_size $num_requests $push_ratio &
done
wait
#################################################
sleep 1
echo "*** PUSH only ***"
push_ratio=1
START_TIME=$(echo $(($(date +%s%N))))
for ((i=0;i<num_clients;i++)); do
  python3 kvstack_bench.py $server_address $value_size $num_requests $push_ratio &
done
wait
END_TIME=$(echo $(($(date +%s%N))))
ELAPSED_TIME=$(($END_TIME - $START_TIME))
elapsed_time_msec=$(echo "scale=1; $ELAPSED_TIME / 1000000" | bc)
echo "Elapsed time: $elapsed_time_msec msec"
THROUGHPUT_1=$(echo "scale=1; $num_clients * $num_requests * 1000 / $elapsed_time_msec" | bc)
echo "Throughput: $THROUGHPUT_1 ops/sec"
echo "-----------------------------------------"
#################################################
sleep 1
echo "*** PUSH:POP = 1:1 ***"
push_ratio=0.5
START_TIME=$(echo $(($(date +%s%N))))
for ((i=0;i<num_clients;i++)); do
  python3 kvstack_bench.py $server_address $value_size $num_requests $push_ratio &
done
wait
END_TIME=$(echo $(($(date +%s%N))))
ELAPSED_TIME=$(($END_TIME - $START_TIME))
elapsed_time_msec=$(echo "scale=1; $ELAPSED_TIME / 1000000" | bc)
echo "Elapsed time: $elapsed_time_msec msec"
THROUGHPUT_2=$(echo "scale=1; $num_clients * $num_requests * 1000 / $elapsed_time_msec" | bc)
echo "Throughput: $THROUGHPUT_2 ops/sec"
echo "-----------------------------------------"
#################################################
sleep 1
echo "*** POP only ***"
push_ratio=0
START_TIME=$(echo $(($(date +%s%N))))
for ((i=0;i<num_clients;i++)); do
  python3 kvstack_bench.py $server_address $value_size $num_requests $push_ratio &
done
wait
END_TIME=$(echo $(($(date +%s%N))))
ELAPSED_TIME=$(($END_TIME - $START_TIME))
elapsed_time_msec=$(echo "scale=1; $ELAPSED_TIME / 1000000" | bc)
echo "Elapsed time: $elapsed_time_msec msec"
THROUGHPUT_3=$(echo "scale=1; $num_clients * $num_requests * 1000 / $elapsed_time_msec" | bc)
echo "Throughput: $THROUGHPUT_3 ops/sec"
echo "-----------------------------------------"
echo "$THROUGHPUT_1	$THROUGHPUT_2	$THROUGHPUT_3"
echo "========================================="
