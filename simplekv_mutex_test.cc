#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

#define SIMPLEKV_MAP_UNORDERED

class SimpleKV {
 public:
  using Value = std::pair<std::string, std::time_t>;

  int Put(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lk(mu_);
    map_[key] = Value(value, std::time(nullptr));
    return 0;
  }

  Value Get(const std::string& key) {
    std::shared_lock<std::shared_mutex> lk(mu_);
    auto it = map_.find(key);
    if (it != map_.end()) {
      return it->second;
    } else {
      return Value("", 0);
    }
  }

  int Del(const std::string& key) {
    std::unique_lock<std::shared_mutex> lk(mu_);
    map_.erase(key);
    return 0;
  }

  bool CAS(const std::string& key, const std::string value, const std::string orig) {
    std::unique_lock<std::shared_mutex> lk(mu_);
    auto it = map_.find(key);
    if (it != map_.end()) {
      if (it->second.first == orig) {
        it->second.first = value;
        it->second.second = std::time(nullptr);
        return true;
      }
    }
    return false;
  }

 private:
#ifdef SIMPLEKV_MAP_UNORDERED
  using Map = std::unordered_map<std::string, Value>;
#else
  using Map = std::map<std::string, Value>;
#endif
  std::shared_mutex mu_;
  Map map_;
};

SimpleKV* kv;
std::mutex global_mu;

inline std::string GetStackNameKey(const std::string& stack_name) {
  return "[]:" + stack_name;
}

inline std::string GetAddressCounterKey(const std::string& stack_name) {
  return "[]:" + stack_name + ":COUNTER";
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

inline std::string GetAddressKey(const std::string& stack_name, const uint64_t addr_num) {
  return GetStackNameKey(stack_name) + ":@" + std::to_string(addr_num);
}

uint64_t ExtractNextAddressNum(const std::string& encoded) {
  const char* p = encoded.data();
  uint16_t value_len = *((const uint16_t*) p);
  p += 2 + value_len;
  uint64_t next_addr = *((const uint64_t*) p);
  return next_addr;
}

void PrintDecode(const std::string& encoded) {
  uint16_t val_len = *((uint16_t*) encoded.data());
  if (val_len == 0) {
    std::cout << "Value=\n";
    return;
  }
  std::cout << "Value=" << val_len << ":";
  for (int i  = 2; i < 2+val_len; i++) {
    std::cout << encoded.data()[i];
  }
  std::cout << ":@" << *((uint64_t*) &(encoded.data()[2+val_len]));
  std::cout << std::endl;
}

int main(const int argc, const char* argv[]) {
  if (argc < 5) {
    fprintf(stderr, "Usage: %s <value_size> <num_loads> <num_requests> <num_clients>\n", argv[0]);
    return 1;
  }
  int value_size = atoi(argv[1]);
  size_t num_loads = atoll(argv[2]);
  int num_requests = atoi(argv[3]);
  int num_clients = atoi(argv[4]);
  int num_total_requests = num_requests * num_clients;

  printf("=========================================\n");
  printf("Benchmark configuration:\n");
  printf("  value_size: %d\n", value_size);
  printf("  num_loads: %zu\n", num_loads);
  printf("  num_requests (per client): %d", num_requests);
  printf("  num_clients: %d\n", num_clients);
  printf("  toal requests: %d\n", num_clients*num_requests);
  printf("-----------------------------------------\n");

  std::vector<double> thpts;

  kv = new SimpleKV();
  {
    std::cout << "*** Load ***\n";
    // Load
    std::string stack_name = "my_stack";
    std::string value = std::string(value_size, 'a');
    for (int i = 0; i < num_loads; i++) {
      std::string key = "[]:" + stack_name + ":@" + std::to_string(i+1);
      kv->Put(key, value);
    }
  }
  {
    // PUT-only
    std::cout << "*** PUT only ***\n";
    float push_ratio = 1.0;
    std::vector<std::thread> clients;
    auto time_begin = std::chrono::system_clock::now();
    for (int i = 0; i < num_clients; i++) {
      clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio, num_total_requests, num_loads] {
            std::mt19937 gen(cid);
            std::uniform_real_distribution<> dis(0.0, 1.0);
            std::uniform_int_distribution<> dis_put(num_loads+1, num_loads+1+num_total_requests);
            int req_done_cnt = 0;
            std::string stack_name = "my_stack";
            std::string value = std::string(value_size, 'a');
            while (req_done_cnt < num_requests) {
              std::string key = "[]:" + stack_name + ":@" + std::to_string(dis_put(gen));
              if (dis(gen) < push_ratio) {
                kv->Put(key, value);
              } else {
                auto ret = kv->Get(key);
              }
              req_done_cnt++;
            }
          });
    }
    for (auto& c : clients) {
      if (c.joinable()) {
        c.join();
      }
    }
    auto time_end = std::chrono::system_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_begin);
    double dur_sec = (double) dur.count() / 1000000;
    double thpt = num_clients * num_requests / dur_sec / 1000;
    thpts.push_back(thpt);
    std::cout << "Elapsed time: " << dur_sec << "sec\n";
    std::cout << "Throughput: " << thpt << " Kops/sec" << std::endl;
    std::cout << "-----------------------------------------\n";
  }
  {
    // half-half
    std::cout << "*** PUT:GET = 1:1 ***\n";
    float push_ratio = 0.5;
    std::vector<std::thread> clients;
    auto time_begin = std::chrono::system_clock::now();
    for (int i = 0; i < num_clients; i++) {
      clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio, num_total_requests, num_loads] {
            std::mt19937 gen(cid);
            std::uniform_real_distribution<> dis(0.0, 1.0);
            std::uniform_int_distribution<> dis_put(num_loads+1+num_total_requests, num_loads+1+2*num_total_requests);
            std::uniform_int_distribution<> dis_get(1, num_loads+1+2*num_total_requests);
            int req_done_cnt = 0;
            std::string stack_name = "my_stack";
            std::string value = std::string(value_size, 'a');
            while (req_done_cnt < num_requests) {
              if (dis(gen) < push_ratio) {
                std::string key = "[]:" + stack_name + ":@" + std::to_string(dis_put(gen));
                kv->Put(key, value);
              } else {
                std::string key = "[]:" + stack_name + ":@" + std::to_string(dis_get(gen));
                auto ret = kv->Get(key);
              }
              req_done_cnt++;
            }
          });
    }
    for (auto& c : clients) {
      if (c.joinable()) {
        c.join();
      }
    }
    auto time_end = std::chrono::system_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_begin);
    double dur_sec = (double) dur.count() / 1000000;
    double thpt = num_clients * num_requests / dur_sec / 1000;
    thpts.push_back(thpt);
    std::cout << "Elapsed time: " << dur_sec << "sec\n";
    std::cout << "Throughput: " << thpt << " Kops/sec" << std::endl;
    std::cout << "-----------------------------------------\n";
  }
  {
    // GET only
    std::cout << "*** GET-only ***\n";
    float push_ratio = 0;
    std::vector<std::thread> clients;
    auto time_begin = std::chrono::system_clock::now();
    for (int i = 0; i < num_clients; i++) {
      clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio, num_total_requests, num_loads] {
            std::mt19937 gen(cid);
            std::uniform_real_distribution<> dis(0.0, 1.0);
            std::uniform_int_distribution<> dis_put(num_loads+1+num_total_requests, num_loads+1+2*num_total_requests);
            std::uniform_int_distribution<> dis_get(1, num_loads+1+2*num_total_requests);
            int req_done_cnt = 0;
            std::string stack_name = "my_stack";
            std::string value = std::string(value_size, 'a');
            while (req_done_cnt < num_requests) {
              if (dis(gen) < push_ratio) {
                std::string key = "[]:" + stack_name + ":@" + std::to_string(dis_put(gen));
                kv->Put(key, value);
              } else {
                std::string key = "[]:" + stack_name + ":@" + std::to_string(dis_get(gen));
                auto ret = kv->Get(key);
              }
              req_done_cnt++;
            }
          });
    }
    for (auto& c : clients) {
      if (c.joinable()) {
        c.join();
      }
    }
    auto time_end = std::chrono::system_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_begin);
    double dur_sec = (double) dur.count() / 1000000;
    double thpt = num_clients * num_requests / dur_sec / 1000;
    thpts.push_back(thpt);
    std::cout << "Elapsed time: " << dur_sec << "sec\n";
    std::cout << "Throughput: " << thpt << " Kops/sec" << std::endl;
    std::cout << "-----------------------------------------\n";
  }
  {
    // Del-only
    std::cout << "*** DEL-only ***\n";
    float push_ratio = 0;
    std::vector<std::thread> clients;
    auto time_begin = std::chrono::system_clock::now();
    for (int i = 0; i < num_clients; i++) {
      clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio, num_total_requests, num_loads] {
            std::mt19937 gen(cid);
            std::uniform_real_distribution<> dis(0.0, 1.0);
            std::uniform_int_distribution<> dis_del(1, num_loads);
            int req_done_cnt = 0;
            std::string stack_name = "my_stack";
            while (req_done_cnt < num_requests) {
              std::string key = "[]:" + stack_name + ":@" + std::to_string(dis_del(gen));
              kv->Del(key);
              req_done_cnt++;
            }
          });
    }
    for (auto& c : clients) {
      if (c.joinable()) {
        c.join();
      }
    }
    auto time_end = std::chrono::system_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_begin);
    double dur_sec = (double) dur.count() / 1000000;
    double thpt = num_clients * num_requests / dur_sec / 1000;
    thpts.push_back(thpt);
    std::cout << "Elapsed time: " << dur_sec << "sec\n";
    std::cout << "Throughput: " << thpt << " Kops/sec" << std::endl;
    std::cout << "-----------------------------------------\n";
  }
  {
    // CAS-only
    std::cout << "*** CAS-only ***\n";
    std::string key = "cas_test_key";
    std::string encoded(8, ((char) 0));
    kv->Put(key, encoded);
    std::vector<std::thread> clients;
    auto time_begin = std::chrono::system_clock::now();
    for (int i = 0; i < num_clients; i++) {
      clients.emplace_back([cid=i+1, value_size, num_requests, num_total_requests, num_loads, key] {
            std::mt19937 gen(cid);
            std::uniform_int_distribution<uint64_t> dis_desired(1, std::numeric_limits<uint64_t>::max());
            int req_done_cnt = 0;
            std::string stack_name = "my_stack";
            std::string desired(8, '0');
            while (req_done_cnt < num_requests) {
              uint64_t desired_int = dis_desired(gen);
              char* p = const_cast<char*>(desired.data());
              memcpy(p, &desired_int, 8);
              auto ret = kv->Get(key);
              while (!kv->CAS(key, desired, ret.first)) {
                ret = kv->Get(key);
              }
              req_done_cnt++;
            }
          });
    }
    for (auto& c : clients) {
      if (c.joinable()) {
        c.join();
      }
    }
    auto time_end = std::chrono::system_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_begin);
    double dur_sec = (double) dur.count() / 1000000;
    double thpt = num_clients * num_requests / dur_sec / 1000;
    thpts.push_back(thpt);
    std::cout << "Elapsed time: " << dur_sec << "sec\n";
    std::cout << "Throughput: " << thpt << " Kops/sec" << std::endl;
    std::cout << "-----------------------------------------\n";
  }
  for (auto thpt : thpts) {
    std::cout << thpt << "	";
  }
  std::cout << std::endl;
  //std::cout << thpts[0] << "	" << thpts[1] << "	" << thpts[2] << "	" << thpts[3] << std::endl;
  printf("=========================================\n");

  return 0;
}
