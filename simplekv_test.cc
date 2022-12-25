#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

#include <cds/init.h>
#include <cds/gc/hp.h>
#include <cds/container/feldman_hashmap_hp.h>
#include <cds/container/details/feldman_hashmap_base.h>

class SimpleKV {
 public:
  int Put(const std::string& key, const std::string& value) {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    if (!map_.emplace(key, value, std::time(nullptr))) {
      auto ret = map_.update(key, [&](map_type::value_type& item, map_type::value_type* old) {
          item.second.first = value;
          item.second.second = std::time(nullptr);
        });
    }
    return 0;
  }

  int PutInt(const std::string& key, const uint64_t value) {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    if (!map_int_.emplace(key, value, std::time(nullptr))) {
      auto ret = map_int_.update(key, [&](map_type_int::value_type& item, map_type_int::value_type* old) {
          item.second.first = value;
          item.second.second = std::time(nullptr);
        });
    }
    return 0;
  }

  int Get(const std::string& key, std::string* value_out) {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    map_type::guarded_ptr gp(map_.get(key));
    if (gp) {
      *value_out = gp->second.first;
      return 0;
    } else {
      *value_out = "";
      return 1;
    }
  }

  int Del(const std::string& key) {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    bool result = map_.erase(key);
    return 0;
  }

  int GetInt(const std::string& key, uint64_t* value_out) {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    map_type_int::guarded_ptr gp(map_int_.get(key));
    if (gp) {
      *value_out = gp->second.first;
      return 0;
    } else {
      *value_out = 0;
      return 1;
    }
  }

  bool CAS(const std::string& key, const uint64_t value, const uint64_t orig) {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    map_type_int::guarded_ptr gp(map_int_.get(key));
    if (gp) {
      uint64_t expected = orig;
      bool result = ((std::atomic<uint64_t>*) &(gp->second.first))->compare_exchange_strong(
          expected, value);
      return result;
    } else {
      return false;
    }
  }

 private:
  using Value = std::pair<std::string, std::time_t>;
  struct string_key_traits: public cds::container::feldman_hashmap::traits {
    typedef std::hash<std::string> hash;
  };

  typedef cds::container::FeldmanHashMap<cds::gc::HP, std::string, std::pair<std::string, std::time_t>, string_key_traits> map_type;
  typedef cds::container::FeldmanHashMap<cds::gc::HP, std::string, std::pair<int64_t, std::time_t>, string_key_traits> map_type_int;
  
  map_type map_;
  map_type_int map_int_;
};

SimpleKV* kv;
std::mutex global_mu;

inline std::string GetStackNameKey(const std::string& stack_name) {
  return "[]:" + stack_name;
}

inline std::string GetAddressCounterKey(const std::string& stack_name) {
  return "[]:" + stack_name + ":COUNTER";
}

uint64_t GetNextAddressNumber(const std::string& stack_name) {
  std::string addr_counter_key = GetAddressCounterKey(stack_name);
  uint64_t cur;
  uint64_t val = 0;
  if (kv->GetInt(addr_counter_key, &cur) == 0) {
    val = cur;
  }
  while (!kv->CAS(addr_counter_key, val + 1, cur)) {
    val = 0;
    if (kv->GetInt(addr_counter_key, &cur) == 0) {
      val = cur;
    }
  }
  return cur + 1;
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

void Push(const std::string& stack_name, const std::string& value) {
  auto stack_name_key = GetStackNameKey(stack_name);
  uint64_t old_top_addr_num = 0;
  if (kv->GetInt(stack_name_key, &old_top_addr_num)) {
    std::unique_lock<std::mutex> lk(global_mu);
    if (kv->GetInt(stack_name_key, &old_top_addr_num)) {
      kv->PutInt(stack_name_key, 0);
      auto addr_counter_key = GetAddressCounterKey(stack_name);
      kv->PutInt(addr_counter_key, 0);
      old_top_addr_num = 0;
    }
  }
  uint64_t new_entry_addr_num = GetNextAddressNumber(stack_name);
  auto new_entry_addr = GetAddressKey(stack_name, new_entry_addr_num);
  auto encoded_value = EncodeValue(value, old_top_addr_num);
  kv->Put(new_entry_addr, encoded_value);
  while (!kv->CAS(stack_name_key, new_entry_addr_num, old_top_addr_num)) {
    int ret = kv->GetInt(stack_name_key, &old_top_addr_num);
    char* p = const_cast<char*>(encoded_value.data());
    p += 2 + value.length();
    memcpy(p, &old_top_addr_num, sizeof(uint64_t));
    kv->Put(new_entry_addr, encoded_value);
  }
}

void PrintTop(const std::string& stack_name) {
  auto stack_name_key = GetStackNameKey(stack_name);
  uint64_t old_top_addr_num;
  if (kv->GetInt(stack_name_key, &old_top_addr_num)) {
    std::unique_lock<std::mutex> lk(global_mu);
    if (kv->GetInt(stack_name_key, &old_top_addr_num)) {
      kv->PutInt(stack_name_key, 0);
      auto addr_counter_key = GetAddressCounterKey(stack_name);
      kv->PutInt(addr_counter_key, 0);
      old_top_addr_num = 0;
    }
  }
  if (old_top_addr_num == 0) {
    std::cout << "Top=" << "NULL\n";
    return;
  }
  auto old_top_addr = GetAddressKey(stack_name, old_top_addr_num);
  std::string value_read;
  if (kv->Get(old_top_addr, &value_read) == 0) {
    uint16_t val_len = *((uint16_t*) value_read.data());
    std::cout << "Top=" << val_len << ":";
    for (int i  = 2; i < 2+val_len; i++) {
      std::cout << value_read.data()[i];
    }
    std::cout << *((uint64_t*) &(value_read.data()[2+val_len]));
    std::cout << std::endl;
  }
}

void PrintStack(const std::string& stack_name) {
  auto stack_name_key = GetStackNameKey(stack_name);
  std::stringstream ss;
  uint64_t top_addr_num;
  if (kv->GetInt(stack_name_key, &top_addr_num)) {
    std::cout << "No stack\n";
    return;
  }
  ss << "TOP";
  uint64_t next_addr_num = top_addr_num;
  while (next_addr_num != 0) {
    auto next_top_addr = GetAddressKey(stack_name, next_addr_num);
    std::string value_read;
    if (kv->Get(next_top_addr, &value_read) == 0) {
      uint16_t val_len = *((uint16_t*) value_read.data());
      ss << "->@" << next_top_addr << "[" << val_len << ":";
      for (int i  = 2; i < 2+val_len; i++) {
        ss << value_read.data()[i];
      }
      next_addr_num = *((uint64_t*) &(value_read.data()[2+val_len]));
      ss << next_addr_num;
      ss << "]";
    } else {
      break;
    }
  }
  ss << "->NULL";
  std::cout << ss.str() << std::endl;
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

void Pop(const std::string& stack_name) {
  auto stack_name_key = GetStackNameKey(stack_name);
  uint64_t old_top_addr_num;
  kv->GetInt(stack_name_key, &old_top_addr_num);
  if (kv->GetInt(stack_name_key, &old_top_addr_num)) {
    std::unique_lock<std::mutex> lk(global_mu);
    if (kv->GetInt(stack_name_key, &old_top_addr_num)) {
      kv->PutInt(stack_name_key, 0);
      auto addr_counter_key = GetAddressCounterKey(stack_name);
      kv->PutInt(addr_counter_key, 0);
      old_top_addr_num = 0;
    }
  }
  if (old_top_addr_num == 0) {
    std::cerr << "Nothing to pop\n";
    return;
  }
  auto old_top_addr = GetAddressKey(stack_name, old_top_addr_num);
  std::string value_read;
  if (kv->Get(old_top_addr, &value_read)) {
    std::cerr << "No key in KV. stack entry address: " << old_top_addr << std::endl;
  }
  uint64_t next_top_addr_num = ExtractNextAddressNum(value_read);
  while (!kv->CAS(stack_name_key, next_top_addr_num, old_top_addr_num)) {
    kv->GetInt(stack_name_key, &old_top_addr_num);
    if (old_top_addr_num == 0) {
      std::cerr << "Nothing to pop\n";
      return;
    }
    old_top_addr = GetAddressKey(stack_name, old_top_addr_num);
    if (kv->Get(old_top_addr, &value_read)) {
      std::cerr << "No key in KV. stack entry address: " << old_top_addr << std::endl;
    }
    next_top_addr_num = ExtractNextAddressNum(value_read);
  }
}

#if 1
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

  cds::Initialize();
  {
    cds::gc::HP hpGC;
    cds::threading::Manager::attachThread();
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
              std::string value_read;
              while (req_done_cnt < num_requests) {
                std::string key = "[]:" + stack_name + ":@" + std::to_string(dis_put(gen));
                if (dis(gen) < push_ratio) {
                  kv->Put(key, value);
                } else {
                  kv->Get(key, &value_read);
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
              std::string value_read;
              while (req_done_cnt < num_requests) {
                if (dis(gen) < push_ratio) {
                  std::string key = "[]:" + stack_name + ":@" + std::to_string(dis_put(gen));
                  kv->Put(key, value);
                } else {
                  std::string key = "[]:" + stack_name + ":@" + std::to_string(dis_get(gen));
                  kv->Get(key, &value_read);
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
              std::string value_read;
              while (req_done_cnt < num_requests) {
                if (dis(gen) < push_ratio) {
                  std::string key = "[]:" + stack_name + ":@" + std::to_string(dis_put(gen));
                  kv->Put(key, value);
                } else {
                  std::string key = "[]:" + stack_name + ":@" + std::to_string(dis_get(gen));
                  kv->Get(key, &value_read);
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
    std::cout << thpts[0] << "	" << thpts[1] << "	" << thpts[2] << std::endl;
    printf("=========================================\n");
  }

  cds::Terminate();
  return 0;
}
#else
int main(const int argc, const char* argv[]) {
  if (argc < 4) {
    fprintf(stderr, "Usage: %s <value_size> <num_requests> <num_clients>\n", argv[0]);
    return 1;
  }
  int value_size = atoi(argv[1]);
  int num_requests = atoi(argv[2]);
  float push_ratio = 1.0;
  int num_clients = atoi(argv[3]);

  cds::Initialize();
  {
    cds::gc::HP hpGC;
    cds::threading::Manager::attachThread();
    {
      // Put prefixed Random
      kv = new SimpleKV();
      std::vector<std::thread> clients;
      for (int i = 0; i < num_clients; i++) {
        clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio] {
              std::mt19937 gen(cid);
              std::uniform_int_distribution<> dis(0, 1000000000);
              int req_done_cnt = 0;
              std::string stack_name = "my_stack";
              std::string value = std::string(value_size, 'a');
              while (req_done_cnt < num_requests) {
                std::string key = "[]:" + stack_name + ":@" + std::to_string(dis(gen));
                kv->Put(key, value);
                req_done_cnt++;
              }
            });
      }
      for (auto& c : clients) {
        if (c.joinable()) {
          c.join();
        }
      }
      delete kv;
    }
    {
      std::cout << "*** Put Prefixed Random ***\n";
      // Put prefixed Random
      kv = new SimpleKV();
      std::vector<std::thread> clients;
      auto time_begin = std::chrono::system_clock::now();
      for (int i = 0; i < num_clients; i++) {
        clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio] {
              std::mt19937 gen(cid);
              std::uniform_int_distribution<> dis(0, 1000000000);
              int req_done_cnt = 0;
              std::string stack_name = "my_stack";
              std::string value = std::string(value_size, 'a');
              while (req_done_cnt < num_requests) {
                std::string key = "[]:" + stack_name + ":@" + std::to_string(dis(gen));
                kv->Put(key, value);
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
      std::cout << dur.count() << "usec\n";
      std::cout << "Throughput: " << num_clients * num_requests / dur_sec / 1000 << " Kops/sec" << std::endl;
      delete kv;
    }
    {
      std::cout << "*** Put Random ***\n";
      // Put Random
      kv = new SimpleKV();
      std::vector<std::thread> clients;
      auto time_begin = std::chrono::system_clock::now();
      for (int i = 0; i < num_clients; i++) {
        clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio] {
              std::mt19937 gen(cid);
              std::uniform_int_distribution<> dis(0, 1000000000);
              int req_done_cnt = 0;
              std::string stack_name = "my_stack";
              std::string value = std::string(value_size, 'a');
              while (req_done_cnt < num_requests) {
                std::string key = std::to_string(dis(gen));
                kv->Put(key, value);
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
      std::cout << dur.count() << "usec\n";
      std::cout << "Throughput: " << num_clients * num_requests / dur_sec / 1000 << " Kops/sec" << std::endl;
      delete kv;
    }
    {
      kv = new SimpleKV();
      std::cout << "*** Put Prefixed Sequential ***\n";
      // Put prefixed Sequential
      push_ratio = 1.0;
      std::vector<std::thread> clients;
      auto time_begin = std::chrono::system_clock::now();
      for (int i = 0; i < num_clients; i++) {
        clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio] {
              int req_done_cnt = 0;
              std::string stack_name = "my_stack";
              std::string value = std::string(value_size, 'a');
              int i = cid * num_requests;
              while (req_done_cnt < num_requests) {
                std::string key = "[]:" + stack_name + ":@" + std::to_string(i++);
                kv->Put(key, value);
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
      std::cout << dur.count() << "usec\n";
      std::cout << "Throughput: " << num_clients * num_requests / dur_sec / 1000 << " Kops/sec" << std::endl;
      delete kv;
    }
  }

  cds::Terminate();
  return 0;
}
#endif
