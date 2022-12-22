#!/usr/bin/env bash
if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <server_address:port> <num_clients>"
  exit 1
fi
server_address=$1
num_clients=$2
server_host=$(echo $server_address | cut -d ":" -f 1)
server_port=$(echo $server_address | cut -d ":" -f 2)
server_exec=$(realpath simplekv_server)
client_exec=$(realpath kvstack_bench.py)
#echo $server_host $server_port
#ssh $server_host "${server_exec}"
for ((i=0;i<num_clients;i++)); do
  python3 kvstack_bench.py $server_address &
done
wait
