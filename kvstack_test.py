#!/usr/bin/env python3

import logging
import re
import sys
from simplekv_client import SimpleKVClient

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

def get_stack_name_key(stackName):
  return "[]:" + stackName

class Stack:
  """Stak"""
  def __init__(self, name):
    self._name = name
    self._name_key = get_stack_name_key(name)
    self._counter_key = self._name_key + ":COUNTER"
    #self._data = data
  #def next(self):
  #  if self._data == 0:
  #    return None
  #  else:
  #    return self._name + "@" + str(self._data)
  def new_node(self):
    before = inc(self._counter_key)
    return self._name_key + ":@" + str(before+1)
  def get_top(self):
    ret, _ = kv.get_int(self._name_key)
    return ret

def get_or_create_stack(stackName):
  if not isinstance(stackName, str):
    return None
  if stackName.startswith("[]:"):
    return None
  key = get_stack_name_key(stackName)
  ret, _ = kv.get_int(key)
  if ret == None:
    while not kv.cas("STACK_CREATION_LOCK", 1, 0):
      continue
    kv.put(key, 0)
    kv.put(key + ":COUNTER", 0)
    while not kv.cas("STACK_CREATION_LOCK", 0, 1):
      continue
    ret, _ = kv.get_int(key)
  return Stack(stackName)

def encode_value(value, next_offset):
  value_len = len(value)
  encoded_len = 2 + value_len + 8
  buf = bytearray(encoded_len)
  buf[:2] = value_len.to_bytes(2, "big", signed=False)
  buf[2:2+value_len] = value.encode()
  if next_offset == None:
    next_offset = 0
  buf[2+value_len:] = next_offset.to_bytes(8, "big", signed=False)
  return buf

def extract_address_offset(kv_address):
  p = re.compile("^\[\]:.*:@(\d+)$")
  result = p.search(kv_address)
  if result != None:
    return int(result.group(1))
  return None

def push(stackName, value):
  """push value to the stack named `stackName`.
  Args:
    stackName: string, the name of the stack.
    value: string, user value.
  Throws:
  """
  if value == None:
    return
  st = get_or_create_stack(stackName)
  if st == None:
    print("get_or_create_stack() returns None")
    return
  new_node_key = st.new_node()
  print("New addr: " + new_node_key)
  new_node_addr_offset = extract_address_offset(new_node_key)
  while True:
    old_top = st.get_top()
    print("Old top: ", old_top)
    #addr_offset = extract_address_offset(old_top)
    encoded_value = encode_value(value, old_top)
    if len(encoded_value) > 32000:
      print("Error: stack entry size cannot exceed 32K.")
      return
    print("Encoded: " + str(encoded_value))
    # TODO: DO THIS WITH NONE VALUE FIRST
    kv.put(new_node_key, encoded_value)
    if not kv.cas(get_stack_name_key(stackName), new_node_addr_offset, old_top):
      continue
    break

def decode(stackName, encoded):
  value_len = int.from_bytes(encoded[:2], "big")
  value = encoded[2:2+value_len].decode()
  next_addr = int.from_bytes(encoded[2+value_len:], "big")
  return value_len, value, next_addr

def pop(stackName):
  """Pop
  Returns:
    None: when the stack is empty or when some problems occur
  """
  st = get_or_create_stack(stackName)
  if st == None:
    print("get_or_create_stack() returns None")
    return
  while True:
    old_top = st.get_top()
    if old_top == "NULL":
      return None
    old_top_addr = "[]:" + stackName + ":@" + str(old_top)
    ret, _ = kv.get(old_top_addr)
    if ret == None:
      return None
    value_len, value, new_top = decode(stackName, bytes(ret))
    if not kv.cas(st._name_key, new_top, old_top):
      continue
    kv.Del(old_top_addr)
    return value

if __name__ == "__main__":
  logging.basicConfig()
  if len(sys.argv) > 1:
    kv = SimpleKVClient(sys.argv[1])
  else:
    kv = SimpleKVClient()
  kv.put("STACK_CREATION_LOCK", 0)

  push("haha", "apple")
  push("haha", "banana")
  push("haha", "cat")
  print("Pop haha", pop("haha"))
  push("haha", "dog")
  print("Pop haha", pop("haha"))
  print("Pop haha", pop("haha"))
  print("Pop haha", pop("haha"))
  print("Pop haha", pop("haha"))
  push("[]:hello", "world")
  print("Pop great", pop("great"))
