# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: simplekv.proto
"""Generated protocol buffer code."""
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n\x0esimplekv.proto\x12\x08simplekv\"(\n\nPutRequest\x12\x0b\n\x03key\x18\x01 \x01(\t\x12\r\n\x05value\x18\x02 \x01(\x0c\"\x1b\n\x08PutReply\x12\x0f\n\x07message\x18\x01 \x01(\t\"+\n\rPutIntRequest\x12\x0b\n\x03key\x18\x01 \x01(\t\x12\r\n\x05value\x18\x02 \x01(\x04\"\x19\n\nGetRequest\x12\x0b\n\x03key\x18\x01 \x01(\t\",\n\x08GetReply\x12\r\n\x05value\x18\x01 \x01(\x0c\x12\x11\n\ttimestamp\x18\x02 \x01(\x04\"/\n\x0bGetIntReply\x12\r\n\x05value\x18\x01 \x01(\x04\x12\x11\n\ttimestamp\x18\x02 \x01(\x04\"\x19\n\nDelRequest\x12\x0b\n\x03key\x18\x01 \x01(\t\"\x1b\n\x08\x44\x65lReply\x12\x0f\n\x07message\x18\x01 \x01(\t\"6\n\nCASRequest\x12\x0b\n\x03key\x18\x01 \x01(\t\x12\r\n\x05value\x18\x02 \x01(\x04\x12\x0c\n\x04orig\x18\x03 \x01(\x04\"\x1b\n\x08\x43\x41SReply\x12\x0f\n\x07success\x18\x01 \x01(\x08\x32\xc8\x02\n\x08SimpleKV\x12\x31\n\x03Put\x12\x14.simplekv.PutRequest\x1a\x12.simplekv.PutReply\"\x00\x12\x37\n\x06PutInt\x12\x17.simplekv.PutIntRequest\x1a\x12.simplekv.PutReply\"\x00\x12\x31\n\x03Get\x12\x14.simplekv.GetRequest\x1a\x12.simplekv.GetReply\"\x00\x12\x37\n\x06GetInt\x12\x14.simplekv.GetRequest\x1a\x15.simplekv.GetIntReply\"\x00\x12\x31\n\x03\x44\x65l\x12\x14.simplekv.DelRequest\x1a\x12.simplekv.DelReply\"\x00\x12\x31\n\x03\x43\x41S\x12\x14.simplekv.CASRequest\x1a\x12.simplekv.CASReply\"\x00\x62\x06proto3')



_PUTREQUEST = DESCRIPTOR.message_types_by_name['PutRequest']
_PUTREPLY = DESCRIPTOR.message_types_by_name['PutReply']
_PUTINTREQUEST = DESCRIPTOR.message_types_by_name['PutIntRequest']
_GETREQUEST = DESCRIPTOR.message_types_by_name['GetRequest']
_GETREPLY = DESCRIPTOR.message_types_by_name['GetReply']
_GETINTREPLY = DESCRIPTOR.message_types_by_name['GetIntReply']
_DELREQUEST = DESCRIPTOR.message_types_by_name['DelRequest']
_DELREPLY = DESCRIPTOR.message_types_by_name['DelReply']
_CASREQUEST = DESCRIPTOR.message_types_by_name['CASRequest']
_CASREPLY = DESCRIPTOR.message_types_by_name['CASReply']
PutRequest = _reflection.GeneratedProtocolMessageType('PutRequest', (_message.Message,), {
  'DESCRIPTOR' : _PUTREQUEST,
  '__module__' : 'simplekv_pb2'
  # @@protoc_insertion_point(class_scope:simplekv.PutRequest)
  })
_sym_db.RegisterMessage(PutRequest)

PutReply = _reflection.GeneratedProtocolMessageType('PutReply', (_message.Message,), {
  'DESCRIPTOR' : _PUTREPLY,
  '__module__' : 'simplekv_pb2'
  # @@protoc_insertion_point(class_scope:simplekv.PutReply)
  })
_sym_db.RegisterMessage(PutReply)

PutIntRequest = _reflection.GeneratedProtocolMessageType('PutIntRequest', (_message.Message,), {
  'DESCRIPTOR' : _PUTINTREQUEST,
  '__module__' : 'simplekv_pb2'
  # @@protoc_insertion_point(class_scope:simplekv.PutIntRequest)
  })
_sym_db.RegisterMessage(PutIntRequest)

GetRequest = _reflection.GeneratedProtocolMessageType('GetRequest', (_message.Message,), {
  'DESCRIPTOR' : _GETREQUEST,
  '__module__' : 'simplekv_pb2'
  # @@protoc_insertion_point(class_scope:simplekv.GetRequest)
  })
_sym_db.RegisterMessage(GetRequest)

GetReply = _reflection.GeneratedProtocolMessageType('GetReply', (_message.Message,), {
  'DESCRIPTOR' : _GETREPLY,
  '__module__' : 'simplekv_pb2'
  # @@protoc_insertion_point(class_scope:simplekv.GetReply)
  })
_sym_db.RegisterMessage(GetReply)

GetIntReply = _reflection.GeneratedProtocolMessageType('GetIntReply', (_message.Message,), {
  'DESCRIPTOR' : _GETINTREPLY,
  '__module__' : 'simplekv_pb2'
  # @@protoc_insertion_point(class_scope:simplekv.GetIntReply)
  })
_sym_db.RegisterMessage(GetIntReply)

DelRequest = _reflection.GeneratedProtocolMessageType('DelRequest', (_message.Message,), {
  'DESCRIPTOR' : _DELREQUEST,
  '__module__' : 'simplekv_pb2'
  # @@protoc_insertion_point(class_scope:simplekv.DelRequest)
  })
_sym_db.RegisterMessage(DelRequest)

DelReply = _reflection.GeneratedProtocolMessageType('DelReply', (_message.Message,), {
  'DESCRIPTOR' : _DELREPLY,
  '__module__' : 'simplekv_pb2'
  # @@protoc_insertion_point(class_scope:simplekv.DelReply)
  })
_sym_db.RegisterMessage(DelReply)

CASRequest = _reflection.GeneratedProtocolMessageType('CASRequest', (_message.Message,), {
  'DESCRIPTOR' : _CASREQUEST,
  '__module__' : 'simplekv_pb2'
  # @@protoc_insertion_point(class_scope:simplekv.CASRequest)
  })
_sym_db.RegisterMessage(CASRequest)

CASReply = _reflection.GeneratedProtocolMessageType('CASReply', (_message.Message,), {
  'DESCRIPTOR' : _CASREPLY,
  '__module__' : 'simplekv_pb2'
  # @@protoc_insertion_point(class_scope:simplekv.CASReply)
  })
_sym_db.RegisterMessage(CASReply)

_SIMPLEKV = DESCRIPTOR.services_by_name['SimpleKV']
if _descriptor._USE_C_DESCRIPTORS == False:

  DESCRIPTOR._options = None
  _PUTREQUEST._serialized_start=28
  _PUTREQUEST._serialized_end=68
  _PUTREPLY._serialized_start=70
  _PUTREPLY._serialized_end=97
  _PUTINTREQUEST._serialized_start=99
  _PUTINTREQUEST._serialized_end=142
  _GETREQUEST._serialized_start=144
  _GETREQUEST._serialized_end=169
  _GETREPLY._serialized_start=171
  _GETREPLY._serialized_end=215
  _GETINTREPLY._serialized_start=217
  _GETINTREPLY._serialized_end=264
  _DELREQUEST._serialized_start=266
  _DELREQUEST._serialized_end=291
  _DELREPLY._serialized_start=293
  _DELREPLY._serialized_end=320
  _CASREQUEST._serialized_start=322
  _CASREQUEST._serialized_end=376
  _CASREPLY._serialized_start=378
  _CASREPLY._serialized_end=405
  _SIMPLEKV._serialized_start=408
  _SIMPLEKV._serialized_end=736
# @@protoc_insertion_point(module_scope)
