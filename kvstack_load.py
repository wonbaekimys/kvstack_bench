#!/usr/bin/env python3

import sys
from simplekv_client import SimpleKVClient

if __name__ == "__main__":
  if len(sys.argv) != 6:
    print("Usage: " + sys.argv[0] + " <server_host:port> <stack_name_prefix> <value_size> <num_loads> <num_stacks>")
    exit(1)
  kv = SimpleKVClient(sys.argv[1])
  prefix = sys.argv[2]
  value = 'a' * int(sys.argv[3])
  num_loads = int(sys.argv[4])
  num_stacks = int(sys.argv[5])
  for i in range(0, num_stacks):
    stack_name = prefix + "_" + str(i+1)
    print("load " + stack_name + ": " + str(int(num_loads/num_stacks)))
    kv.load(stack_name, value, int(num_loads/num_stacks))

