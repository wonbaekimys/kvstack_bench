#!/usr/bin/env python3

import random
import re
import sys
from simplekv_client import SimpleKVClient

def get_stack_name_key(stackName):
  return "[]:" + stackName

def get_address_counter_key(stackName):
  return get_stack_name_key(stackName) + ":COUNTER"

def maybe_create_stack(stackName):
  if not isinstance(stackName, str):
    raise Exception("stackName is not str.")
  if stackName.startswith("[]:"):
    raise Exception("stackName starts with \"[]:\"")
  stack_name_key = get_stack_name_key(stackName)
  addr_counter_key = get_address_counter_key(stackName)
  ret, _ = kv.get_int(stack_name_key)
  if ret == None:
    while not kv.cas("STACK_CREATION_LOCK", 1, 0):
      continue
    kv.put(stack_name_key, 0)
    kv.put(addr_counter_key, 0)
    while not kv.cas("STACK_CREATION_LOCK", 0, 1):
      continue

def encode_value(value, addr_num):
  value_len = len(value)
  encoded_len = 2 + value_len + 8
  buf = bytearray(encoded_len)
  buf[:2] = value_len.to_bytes(2, "big", signed=False)
  buf[2:2+value_len] = value.encode()
  if addr_num == None:
    addr_num = 0
  buf[2+value_len:] = addr_num.to_bytes(8, "big", signed=False)
  return buf

def get_address_key(stackName, addr_num):
  return get_stack_name_key(stackName) + ":@" + str(addr_num)

def extract_address_num(kv_address):
  p = re.compile("^\[\]:.*:@(\d+)$")
  result = p.search(kv_address)
  if result != None:
    return int(result.group(1))
  return None

def decode_value(encoded):
  value_len = int.from_bytes(encoded[:2], "big")
  value = encoded[2:2+value_len].decode()
  next_addr = int.from_bytes(encoded[2+value_len:], "big")
  return value_len, value, next_addr

def extract_next_address_num(encoded):
  value_len = int.from_bytes(encoded[:2], "big")
  next_addr = int.from_bytes(encoded[2+value_len:], "big")
  return next_addr

def inc(key):
  cur, _ = kv.get_int(key)
  val = 0
  if cur != None:
    val = cur
  while not kv.cas(key, val+1, cur):
    cur, _ = kv.get_int(key)
    val = 0
    if cur != None:
      val = cur
  return cur

def get_next_address_number(stackName):
  addr_counter_key = get_address_counter_key(stackName)
  before = inc(addr_counter_key)
  return before + 1

def push(stackName, value):
  if value == None:
    raise Exception("value is NULL")
  if not isinstance(stackName, str):
    raise Exception("stackName is not str.")
  if stackName.startswith("[]:"):
    raise Exception("stackName starts with \"[]:\"")
  if len(value) > 31990:
    raise Exception("stack entry size exceeded 32K.")
  # Try to get current top address.
  stack_name_key = get_stack_name_key(stackName)
  old_top_addr_num, _ = kv.get_int(stack_name_key)
  if old_top_addr_num == None:
    # The stack does not exist. Creat a new one.
    while not kv.cas("STACK_CREATION_LOCK", 1, 0):
      continue
    old_top_addr_num, _ = kv.get_int(stack_name_key)
    if old_top_addr_num == None:
      kv.put(stack_name_key, 0)
      addr_counter_key = get_address_counter_key(stackName)
      kv.put(addr_counter_key, 0)
      old_top_addr_num = 0
    while not kv.cas("STACK_CREATION_LOCK", 0, 1):
      continue
  # Allocate address for the new stack entry from the address counter
  new_entry_addr_num = get_next_address_number(stackName)
  new_entry_addr = get_address_key(stackName, new_entry_addr_num)
  # Encode value: user_value + old_top_addr_num
  encoded_value = encode_value(value, old_top_addr_num)
  # Put
  kv.put(new_entry_addr, encoded_value)
  # Try CAS top
  while not kv.cas(stack_name_key, new_entry_addr_num, old_top_addr_num):
    old_top_addr_num, _ = kv.get_int(stack_name_key)
    encoded_value[2+len(value):] = old_top_addr_num.to_bytes(8, "big", signed=False)
    kv.put(new_entry_addr, encoded_value)

def pop(stackName):
  # Try to get current top address.
  stack_name_key = get_stack_name_key(stackName)
  old_top_addr_num, _ = kv.get_int(stack_name_key)
  if old_top_addr_num == None:
    # The stack does not exist. Creat a new one.
    while not kv.cas("STACK_CREATION_LOCK", 1, 0):
      continue
    old_top_addr_num, _ = kv.get_int(stack_name_key)
    if old_top_addr_num == None:
      kv.put(stack_name_key, 0)
      addr_counter_key = get_address_counter_key(stackName)
      kv.put(addr_counter_key, 0)
      old_top_addr_num = 0
    while not kv.cas("STACK_CREATION_LOCK", 0, 1):
      continue
  if old_top_addr_num == 0:
    return
  # Get current top
  old_top_addr = get_address_key(stackName, old_top_addr_num)
  ret, _ = kv.get(old_top_addr)
  if ret == None:
    # error
    raise Exception("No key in KV. stack entry address: " + old_top_addr)
  next_top_addr_num = extract_next_address_num(bytes(ret))
  # Try CAS top
  while not kv.cas(stack_name_key, next_top_addr_num, old_top_addr_num):
    old_top_addr_num, _ = kv.get_int(stack_name_key)
    if old_top_addr_num == 0:
      return
    old_top_addr = get_address_key(stackName, old_top_addr_num)
    ret, _ = kv.get(old_top_addr)
    if ret == None:
      raise Exception("No key in KV. stack entry address: " + old_top_addr)
    next_top_addr_num = extract_next_address_num(bytes(ret))

if __name__ == "__main__":
  if len(sys.argv) < 5:
    print("Usage: " + sys.argv[0] + " <server_address> <value_size> <num_requests> <push_ratio>")
    exit(1)

  kv = SimpleKVClient(sys.argv[1])
  benchmark_value_size = int(sys.argv[2])
  benchamrk_num_requests = int(sys.argv[3])
  benchmark_push_ratio = float(sys.argv[4])

  stack_name = "my_stack"
  value = 'a' * benchmark_value_size

  req_done_cnt = 0
  while req_done_cnt < benchamrk_num_requests:
    # ....
    if random.random() < benchmark_push_ratio:
      push(stack_name, value)
    else:
      pop(stack_name)
    req_done_cnt += 1
