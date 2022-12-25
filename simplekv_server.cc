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
#if 1
    if (!map_.emplace(req->key(), req->value(), std::time(nullptr))) {
      auto ret = map_.update(req->key(), [&](map_type::value_type& item, map_type::value_type* old) {
          item.second.first = req->value();
          item.second.second = std::time(nullptr);
        });
    }
#else
    auto ret = map_.update(req->key(), [&](map_type::value_type& item, map_type::value_type* old) {
        item.second.first = req->value();
        item.second.second = std::time(nullptr);
      });
#endif
    return grpc::Status::OK;
  }

  grpc::Status PutInt(grpc::ServerContext* ctx, const simplekv::PutIntRequest* req,
                      simplekv::PutReply* rep) override {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
#if 1
    if (!map_int_.emplace(req->key(), req->value(), std::time(nullptr))) {
      auto ret = map_int_.update(req->key(), [&](map_type_int::value_type& item, map_type_int::value_type* old) {
          item.second.first = req->value();
          item.second.second = std::time(nullptr);
        });
    }
#else
    auto ret = map_int_.update(req->key(), [&](map_type_int::value_type& item, map_type_int::value_type* old) {
        item.second.first = req->value();
        item.second.second = std::time(nullptr);
      });
#endif
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

  grpc::Status Del(grpc::ServerContext* ctx, const simplekv::DelRequest* req,
                   simplekv::DelReply* rep) override {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    bool result = map_.erase(req->key());
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

  std::string EncodeValue(const std::string& value, const uint64_t addr_num) {
    uint16_t value_len = value.length();
    size_t encoded_len = 2 + value_len + sizeof(uint64_t);
    auto buf = std::string(encoded_len, 'X');
    char* p = const_cast<char*>(buf.data());
    memcpy(p, &value_len, sizeof(uint16_t));
    p += sizeof(uint16_t);
    memcpy(p, value.data(), value_len);
    p += value_len;
    memcpy(p, &addr_num, sizeof(uint64_t));
    return buf;
  }

  grpc::Status Load(grpc::ServerContext* ctx, const simplekv::LoadRequest* req,
                    simplekv::PutReply* rep) override {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    uint64_t old_top_addr_num = 0;
    // Create table if not existing
    map_type_int::guarded_ptr gp(map_int_.get(req->stack_key()));
    if (!gp) {
      // Primary
      map_int_.emplace(req->stack_key(), 0, std::time(nullptr));
      // Alloc Counter
      map_int_.emplace(req->stack_counter_key(), 0, std::time(nullptr));
    }

    for (int i = 0; i < req->num_loads(); i++) {
      if (auto gp = map_type_int::guarded_ptr(map_int_.get(req->stack_key()))) {
        old_top_addr_num = gp->second.first;
      }
      // alloc counter
      uint64_t new_entry_addr_num = 0;
      if (auto gp = map_type_int::guarded_ptr(map_int_.get(req->stack_counter_key()))) {
        auto before = ((std::atomic<uint64_t>*) &gp->second.first)->fetch_add(1);
        new_entry_addr_num = before + 1;
      }
      // new entry
      auto new_entry_addr = req->stack_key() + ":@" + std::to_string(new_entry_addr_num);
      auto encoded_value = EncodeValue(req->value(), old_top_addr_num);
      auto ret = map_.update(new_entry_addr, [&](map_type::value_type& item, map_type::value_type* old) {
          item.second.first = encoded_value;
          item.second.second = std::time(nullptr);
        });
      while (true) {
        bool result = false;
        if (auto gp = map_type_int::guarded_ptr(map_int_.get(req->stack_key()))) {
          uint64_t expected = old_top_addr_num;
          result = ((std::atomic<uint64_t>*) &gp->second.first)->compare_exchange_strong(
              expected, new_entry_addr_num);
        }
        if (result) {
          //std::cout << i << "key: " << new_entry_addr << ", value: " << encoded_value << std::endl;
          break;
        }
        if (auto gp = map_type_int::guarded_ptr(map_int_.get(req->stack_key()))) {
          old_top_addr_num = gp->second.first;
        }
        char* p = const_cast<char*>(encoded_value.data());
        p += 2 + req->value().length();
        memcpy(p, &old_top_addr_num, sizeof(uint64_t));
        ret = map_.update(new_entry_addr, [&](map_type::value_type& item, map_type::value_type* old) {
            item.second.first = encoded_value;
            item.second.second = std::time(nullptr);
          });
      }
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
