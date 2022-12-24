#include <memory>
#include <string>

#include <cds/init.h>
#include <cds/gc/hp.h>
#include <cds/container/feldman_hashmap_hp.h>
#include <cds/container/details/feldman_hashmap_base.h>

#include <grpcpp/grpcpp.h>

#include "simplekv.grpc.pb.h"

class SimpleKVServiceImpl final : public simplekv::SimpleKV::Service {
 public:
  grpc::Status Put(grpc::ServerContext* ctx, const simplekv::PutRequest* req,
                   simplekv::PutReply* rep) override {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    if (!map_.emplace(req->key(), req->value(), std::time(nullptr))) {
      auto ret = map_.update(req->key(), [&](map_type::value_type& item, map_type::value_type* old) {
          item.second.first = req->value();
          item.second.second = std::time(nullptr);
        });
    }
    return grpc::Status::OK;
  }

  grpc::Status Get(grpc::ServerContext* ctx, const simplekv::GetRequest* req,
                   simplekv::GetReply* rep) override {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    map_type::guarded_ptr gp(map_.get(req->key()));
    if (gp) {
      rep->set_value(gp->second.first);
      rep->set_timestamp(gp->second.second);
    } else {
      rep->set_value("");
      rep->set_timestamp(0);
    }
    return grpc::Status::OK;
  }

  grpc::Status Del(grpc::ServerContext* ctx, const simplekv::DelRequest* req,
                   simplekv::DelReply* rep) override {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    bool result = map_.erase(req->key());
    return grpc::Status::OK;
  }

  grpc::Status PutInt(grpc::ServerContext* ctx, const simplekv::PutIntRequest* req,
                      simplekv::PutReply* rep) override {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    if (!map_int_.emplace(req->key(), req->value(), std::time(nullptr))) {
      auto ret = map_int_.update(req->key(), [&](map_type_int::value_type& item, map_type_int::value_type* old) {
          item.second.first = req->value();
          item.second.second = std::time(nullptr);
        });
    }
    return grpc::Status::OK;
  }

  grpc::Status GetInt(grpc::ServerContext* ctx, const simplekv::GetRequest* req,
                      simplekv::GetIntReply* rep) override {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    map_type_int::guarded_ptr gp(map_int_.get(req->key()));
    if (gp) {
      rep->set_value(gp->second.first);
      rep->set_timestamp(gp->second.second);
    } else {
      rep->set_value(0);
      rep->set_timestamp(0);
    }
    return grpc::Status::OK;
  }

  grpc::Status CAS(grpc::ServerContext* ctx, const simplekv::CASRequest* req,
                   simplekv::CASReply* rep) override {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    map_type_int::guarded_ptr gp(map_int_.get(req->key()));
    if (gp) {
      uint64_t expected = req->orig();
      bool result = ((std::atomic<uint64_t>*) &gp->second.first)->compare_exchange_strong(
          expected, req->value());
      rep->set_success(result);
    } else {
      rep->set_success(false);
    }
    return grpc::Status::OK;
  }

 private:
  using Value = std::pair<std::string, std::time_t>;
  struct string_key_traits: public cds::container::feldman_hashmap::traits {
    typedef std::hash<std::string> hash;
  };

  typedef cds::container::FeldmanHashMap<cds::gc::HP, std::string, std::pair<std::string, std::time_t>, string_key_traits> map_type;
  typedef cds::container::FeldmanHashMap<cds::gc::HP, std::string, std::pair<uint64_t, std::time_t>, string_key_traits> map_type_int;
  
  map_type map_;
  map_type_int map_int_;
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  SimpleKVServiceImpl service;

  grpc::ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case, it corresponds to an *synchronous* service.
  builder.RegisterService(&service);

  int physical_cpus = std::thread::hardware_concurrency();
  int max_pollers = std::ceil(physical_cpus / 2);
  int num_cqs = std::ceil(max_pollers / 2);
  builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::NUM_CQS, num_cqs);
  builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::MAX_POLLERS, max_pollers);

  // Finally assemble the server.
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(const int argc, const char* argv[]) {
  cds::Initialize();

  {
    // Initialize Hazard Pointer singleton
    cds::gc::HP hpGC;
    // If main thread uses lock-free containers
    // the main thread should be attached to libcds infrastructure
    cds::threading::Manager::attachThread();
    // Now you can use HP-based containers in the main thread
    //...

    RunServer();
  }

  // Terminate libcds
  cds::Terminate();

  return 0;
}
