# Generated by the gRPC Python protocol compiler plugin. DO NOT EDIT!
"""Client and server classes corresponding to protobuf-defined services."""
import grpc

import simplekv_pb2 as simplekv__pb2


class SimpleKVStub(object):
    """Missing associated documentation comment in .proto file."""

    def __init__(self, channel):
        """Constructor.

        Args:
            channel: A grpc.Channel.
        """
        self.Put = channel.unary_unary(
                '/simplekv.SimpleKV/Put',
                request_serializer=simplekv__pb2.PutRequest.SerializeToString,
                response_deserializer=simplekv__pb2.PutReply.FromString,
                )
        self.PutInt = channel.unary_unary(
                '/simplekv.SimpleKV/PutInt',
                request_serializer=simplekv__pb2.PutIntRequest.SerializeToString,
                response_deserializer=simplekv__pb2.PutReply.FromString,
                )
        self.Get = channel.unary_unary(
                '/simplekv.SimpleKV/Get',
                request_serializer=simplekv__pb2.GetRequest.SerializeToString,
                response_deserializer=simplekv__pb2.GetReply.FromString,
                )
        self.GetInt = channel.unary_unary(
                '/simplekv.SimpleKV/GetInt',
                request_serializer=simplekv__pb2.GetRequest.SerializeToString,
                response_deserializer=simplekv__pb2.GetIntReply.FromString,
                )
        self.Del = channel.unary_unary(
                '/simplekv.SimpleKV/Del',
                request_serializer=simplekv__pb2.DelRequest.SerializeToString,
                response_deserializer=simplekv__pb2.DelReply.FromString,
                )
        self.CAS = channel.unary_unary(
                '/simplekv.SimpleKV/CAS',
                request_serializer=simplekv__pb2.CASRequest.SerializeToString,
                response_deserializer=simplekv__pb2.CASReply.FromString,
                )


class SimpleKVServicer(object):
    """Missing associated documentation comment in .proto file."""

    def Put(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def PutInt(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def Get(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def GetInt(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def Del(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def CAS(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')


def add_SimpleKVServicer_to_server(servicer, server):
    rpc_method_handlers = {
            'Put': grpc.unary_unary_rpc_method_handler(
                    servicer.Put,
                    request_deserializer=simplekv__pb2.PutRequest.FromString,
                    response_serializer=simplekv__pb2.PutReply.SerializeToString,
            ),
            'PutInt': grpc.unary_unary_rpc_method_handler(
                    servicer.PutInt,
                    request_deserializer=simplekv__pb2.PutIntRequest.FromString,
                    response_serializer=simplekv__pb2.PutReply.SerializeToString,
            ),
            'Get': grpc.unary_unary_rpc_method_handler(
                    servicer.Get,
                    request_deserializer=simplekv__pb2.GetRequest.FromString,
                    response_serializer=simplekv__pb2.GetReply.SerializeToString,
            ),
            'GetInt': grpc.unary_unary_rpc_method_handler(
                    servicer.GetInt,
                    request_deserializer=simplekv__pb2.GetRequest.FromString,
                    response_serializer=simplekv__pb2.GetIntReply.SerializeToString,
            ),
            'Del': grpc.unary_unary_rpc_method_handler(
                    servicer.Del,
                    request_deserializer=simplekv__pb2.DelRequest.FromString,
                    response_serializer=simplekv__pb2.DelReply.SerializeToString,
            ),
            'CAS': grpc.unary_unary_rpc_method_handler(
                    servicer.CAS,
                    request_deserializer=simplekv__pb2.CASRequest.FromString,
                    response_serializer=simplekv__pb2.CASReply.SerializeToString,
            ),
    }
    generic_handler = grpc.method_handlers_generic_handler(
            'simplekv.SimpleKV', rpc_method_handlers)
    server.add_generic_rpc_handlers((generic_handler,))


 # This class is part of an EXPERIMENTAL API.
class SimpleKV(object):
    """Missing associated documentation comment in .proto file."""

    @staticmethod
    def Put(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/simplekv.SimpleKV/Put',
            simplekv__pb2.PutRequest.SerializeToString,
            simplekv__pb2.PutReply.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def PutInt(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/simplekv.SimpleKV/PutInt',
            simplekv__pb2.PutIntRequest.SerializeToString,
            simplekv__pb2.PutReply.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def Get(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/simplekv.SimpleKV/Get',
            simplekv__pb2.GetRequest.SerializeToString,
            simplekv__pb2.GetReply.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def GetInt(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/simplekv.SimpleKV/GetInt',
            simplekv__pb2.GetRequest.SerializeToString,
            simplekv__pb2.GetIntReply.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def Del(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/simplekv.SimpleKV/Del',
            simplekv__pb2.DelRequest.SerializeToString,
            simplekv__pb2.DelReply.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def CAS(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/simplekv.SimpleKV/CAS',
            simplekv__pb2.CASRequest.SerializeToString,
            simplekv__pb2.CASReply.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)