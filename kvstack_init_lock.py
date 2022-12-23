#!/usr/bin/env python3

import sys
from simplekv_client import SimpleKVClient

if __name__ == "__main__":
  if len(sys.argv) > 1:
    kv = SimpleKVClient(sys.argv[1])
  else:
    kv = SimpleKVClient()
  kv.put("STACK_CREATION_LOCK", 0)
