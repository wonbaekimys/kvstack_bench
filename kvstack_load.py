#!/usr/bin/env python3

import sys
from simplekv_client import SimpleKVClient

if __name__ == "__main__":
  if len(sys.argv) != 5:
    print("Usage: " + sys.argv[0] + " <server_host:port> <stack_name> <value_size> <num_loads>")
    exit(1)
  kv = SimpleKVClient(sys.argv[1])
  value = 'a' * int(sys.argv[3])
  kv.load(sys.argv[2], value, int(sys.argv[4]))

