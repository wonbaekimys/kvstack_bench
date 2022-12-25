import grpc
import simplekv_pb2
import simplekv_pb2_grpc

class SimpleKVClient:
  def __init__(self, address="0.0.0.0:50051"):
    self._channel = grpc.insecure_channel(address)
    self._stub = simplekv_pb2_grpc.SimpleKVStub(self._channel)

  def put(self, key, value):
    if isinstance(value, int):
      res = self._stub.PutInt(simplekv_pb2.PutIntRequest(key=key, value=value))
    elif isinstance(value, str):
      res = self._stub.Put(simplekv_pb2.PutRequest(key=key, value=value))
    elif isinstance(value, bytearray):
      res = self._stub.Put(simplekv_pb2.PutRequest(key=key, value=bytes(value)))

  def get(self, key):
    res = self._stub.Get(simplekv_pb2.GetRequest(key=key))
    if res.value == "" and res.timestamp == 0:
      return None, 0
    return bytes(res.value), res.timestamp

  def get_int(self, key):
    res = self._stub.GetInt(simplekv_pb2.GetRequest(key=key))
    if res.value == 0 and res.timestamp == 0:
      return None, 0
    return res.value, res.timestamp

  def Del(self, key):
    res = self._stub.Del(simplekv_pb2.DelRequest(key=key))

  def cas(self, key, value, orig):
    res = self._stub.CAS(simplekv_pb2.CASRequest(key=key, value=value, orig=orig))
    return res.success

  def load(self, stack_name, value, num_loads):
    stack_name_key = "[]:" + stack_name
    stack_counter_key = stack_name_key + ":COUNTER"
    res = self._stub.Load(simplekv_pb2.LoadRequest(stack_key=stack_name_key, stack_counter_key=stack_counter_key, value=value, num_loads=num_loads))
