#!/usr/bin/env python3

import re

class KV:
  def __init__(self):
    self._dict = dict()

  def put(self, key, value):
    self._dict[key] = value

  def get(self, key):
    ret = self._dict.get(key)
    if ret == None:
      return None, 0
    return ret, 1

  def cas(self, key, value, orig):
    """Not a true CAS function"""
    ret = self._dict.get(key)
    if ret == None or ret != orig:
      return False
    else:
      self._dict[key] = value
      return True

kv = KV()
kv.put("STACK_CREATION_LOCK", 0)

def inc(key):
  cur, _ = kv.get(key)
  val = 0
  if cur != None:
    val = cur
  while not kv.cas(key, val+1, cur):
    cur, _ = kv.get(key)
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
    ret, _ = kv.get(self._name_key)
    return ret

def get_or_create_stack(stackName):
  if not isinstance(stackName, str):
    return None
  if stackName.startswith("[]:"):
    return None
  key = get_stack_name_key(stackName)
  ret, _ = kv.get(key)
  if ret == None:
    while not kv.cas("STACK_CREATION_LOCK", key, 0):
      continue
    kv.put(key, "NULL")
    kv.put(key + ":COUNTER", 0)
    while not kv.cas("STACK_CREATION_LOCK", 0, key):
      continue
    ret, _ = kv.get(key)
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
  while True:
    old_top = st.get_top()
    print("Old top: ", old_top)
    addr_offset = extract_address_offset(old_top)
    encoded_value = encode_value(value, addr_offset)
    if len(encoded_value) > 32000:
      print("Error: stack entry size cannot exceed 32K.")
      return
    print("Encoded: " + encoded_value.decode())
    # TODO: DO THIS WITH NONE VALUE FIRST
    kv.put(new_node_key, encoded_value)
    if not kv.cas(get_stack_name_key(stackName), new_node_key, old_top):
      continue
    break

def decode(stackName, encoded):
  value_len = int.from_bytes(encoded[:2], "big")
  value = encoded[2:2+value_len].decode()
  next_addr = int.from_bytes(encoded[2+value_len:], "big")
  if next_addr == 0:
    return value_len, value, "[]:" + stackName + ":@NULL"
  else:
    return value_len, value, "[]:" + stackName + ":@" + str(next_addr)

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
    ret, _ = kv.get(old_top)
    if ret == None:
      return None
    value_len, value, new_top = decode(stackName, ret)
    if not kv.cas(st._name_key, new_top, old_top):
      continue
    return value

if __name__ == "__main__":
  push("haha", "123")
  push("haha", "456")
  push("haha", "789")
  print("Pop haha", pop("haha"))
  push("haha", "xxx")
  print("Pop haha", pop("haha"))
  print("Pop haha", pop("haha"))
  print("Pop haha", pop("haha"))
  print("Pop haha", pop("haha"))
  push("0", "456")
  push(0, "789")
  push("[]:hello", "54535")
  print(pop("great"))
