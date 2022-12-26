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

std::mutex global_mu;

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

class SimpleKV {
 public:
  int Put(const std::string& key, const std::string& value) {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
#if 1
    if (!map_.emplace(key, value, std::time(nullptr))) {
      auto ret = map_.update(key, [&](map_type::value_type& item, map_type::value_type* old) {
          item.second.first = value;
          item.second.second = std::time(nullptr);
        });
    }
#else
    auto ret = map_.update(key, [&](map_type::value_type& item, map_type::value_type* old) {
        item.second.first = value;
        item.second.second = std::time(nullptr);
      });
#endif
    return 0;
  }

  int PutInt(const std::string& key, const uint64_t value) {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
#if 1
    if (!map_int_.emplace(key, value, std::time(nullptr))) {
      auto ret = map_int_.update(key, [&](map_type_int::value_type& item, map_type_int::value_type* old) {
          item.second.first = value;
          item.second.second = std::time(nullptr);
        });
    }
#else
    auto ret = map_int_.update(key, [&](map_type_int::value_type& item, map_type_int::value_type* old) {
        item.second.first = value;
        item.second.second = std::time(nullptr);
      });
#endif
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

  int Load(const std::string& stack_name_key, const std::string& addr_counter_key,
           const std::string& value, const size_t num_loads) {
    // Attach the thread if it is not attached yet
    if (!cds::threading::Manager::isThreadAttached()) {
      cds::threading::Manager::attachThread();
    }
    uint64_t old_top_addr_num = 0;
    // Create table if not existing
    map_type_int::guarded_ptr gp(map_int_.get(stack_name_key));
    if (!gp) {
      std::unique_lock<std::mutex> lk(global_mu);
      // Primary
      map_int_.emplace(stack_name_key, 0, std::time(nullptr));
      // Alloc Counter
      map_int_.emplace(addr_counter_key, 0, std::time(nullptr));
    }

    for (int i = 0; i < num_loads; i++) {
      if (auto gp = map_type_int::guarded_ptr(map_int_.get(stack_name_key))) {
        old_top_addr_num = gp->second.first;
      }
      // alloc counter
      uint64_t new_entry_addr_num = 0;
      if (auto gp = map_type_int::guarded_ptr(map_int_.get(addr_counter_key))) {
        auto before = ((std::atomic<uint64_t>*) &gp->second.first)->fetch_add(1);
        new_entry_addr_num = before + 1;
      }
      // new entry
      auto new_entry_addr = stack_name_key + ":@" + std::to_string(new_entry_addr_num);
      auto encoded_value = EncodeValue(value, old_top_addr_num);
      auto ret = map_.update(new_entry_addr, [&](map_type::value_type& item, map_type::value_type* old) {
          item.second.first = encoded_value;
          item.second.second = std::time(nullptr);
        });
      while (true) {
        bool result = false;
        if (auto gp = map_type_int::guarded_ptr(map_int_.get(stack_name_key))) {
          uint64_t expected = old_top_addr_num;
          result = ((std::atomic<uint64_t>*) &gp->second.first)->compare_exchange_strong(
              expected, new_entry_addr_num);
        }
        if (result) {
          //std::cout << i << "key: " << new_entry_addr << ", value: " << encoded_value << std::endl;
          break;
        }
        if (auto gp = map_type_int::guarded_ptr(map_int_.get(stack_name_key))) {
          old_top_addr_num = gp->second.first;
        }
        char* p = const_cast<char*>(encoded_value.data());
        p += 2 + value.length();
        memcpy(p, &old_top_addr_num, sizeof(uint64_t));
        ret = map_.update(new_entry_addr, [&](map_type::value_type& item, map_type::value_type* old) {
            item.second.first = encoded_value;
            item.second.second = std::time(nullptr);
          });
      }
    }
    return 0;
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

inline std::string GetStackNameKey(const std::string& stack_name) {
  return "[]:" + stack_name;
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

void Pop(const std::string& stack_name, std::string* value_out) {
#if 0
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
  const char* p = value_read.data();
  uint16_t value_len = *((const uint16_t*) p);
  p += 2;
  *value_out = std::string(p, value_len);
  kv->Del(old_top_addr);
#else
  auto stack_name_key = GetStackNameKey(stack_name);
  uint64_t old_top_addr_num;
  kv->GetInt(stack_name_key, &old_top_addr_num);
  while (old_top_addr_num != 0) {
    auto old_top_addr = GetAddressKey(stack_name, old_top_addr_num);
    std::string value_read;
    if (kv->Get(old_top_addr, &value_read)) {
      kv->GetInt(stack_name_key, &old_top_addr_num);
      continue;
    }
    uint64_t next_top_addr_num = ExtractNextAddressNum(value_read);
    if (!kv->CAS(stack_name_key, next_top_addr_num, old_top_addr_num)) {
      kv->GetInt(stack_name_key, &old_top_addr_num);
      continue;
    } else {
      const char* p = value_read.data();
      uint16_t value_len = *((const uint16_t*) p);
      p += 2;
      *value_out = std::string(p, value_len);
      kv->Del(old_top_addr);
      break;
    }
  }
  if (old_top_addr_num == 0) {
    std::cerr << "Nothing to pop\n";
    return;
  }
#endif
}

int main(const int argc, const char* argv[]) {
  if (argc < 6) {
    fprintf(stderr, "Usage: %s <value_size> <num_loads> <num_requests> <num_clients> <num_stacks>\n", argv[0]);
    return 1;
  }
  int value_size = atoi(argv[1]);
  size_t num_loads = atoll(argv[2]);
  int num_requests = atoi(argv[3]);
  int num_clients = atoi(argv[4]);
  int num_stacks = atoi(argv[5]);

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
#if 1
      std::vector<std::thread> loaders;
      for (int i = 0; i < num_stacks; i++) {
        loaders.emplace_back([&, cid=i+1]{
              std::string stack_name = "my_stack_" + std::to_string(cid);
              std::string value = std::string(value_size, 'a');
              kv->Load(GetStackNameKey(stack_name), GetAddressCounterKey(stack_name), value, num_loads / num_stacks);
            });
      }
      for (auto& t : loaders) {
        if (t.joinable()) {
          t.join();
        }
      }
#else
      for (int i = 0; i < num_stacks; i++) {
        std::string stack_name = "my_stack_" + std::to_string(i+1);
        std::string value = std::string(value_size, 'a');
        kv->Load(GetStackNameKey(stack_name), GetAddressCounterKey(stack_name), value, num_loads/num_stacks);
      }
#endif
    }
    {
      std::cout << "*** PUSH only ***\n";
      // PUSH-only
      float push_ratio = 1.0;
      std::vector<std::thread> clients;
      auto time_begin = std::chrono::system_clock::now();
      for (int i = 0; i < num_clients; i++) {
        clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio, num_stacks] {
              std::mt19937 gen(cid);
              std::uniform_real_distribution<> dis(0.0, 1.0);
              std::uniform_int_distribution<> dis_stack(1, num_stacks);
              int req_done_cnt = 0;
              std::string value = std::string(value_size, 'a');
              std::string value_read;
              while (req_done_cnt < num_requests) {
                std::string stack_name = "my_stack_" + std::to_string(dis_stack(gen));
                if (dis(gen) < push_ratio) {
                  Push(stack_name, value);
                } else {
                  Pop(stack_name, &value_read);
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
      std::cout << "*** PUSH:POP = 1:1 ***\n";
      // half-half
      float push_ratio = 0.5;
      std::vector<std::thread> clients;
      auto time_begin = std::chrono::system_clock::now();
      for (int i = 0; i < num_clients; i++) {
        clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio, num_stacks] {
              std::mt19937 gen(cid);
              std::uniform_real_distribution<> dis(0.0, 1.0);
              std::uniform_int_distribution<> dis_stack(1, num_stacks);
              int req_done_cnt = 0;
              std::string value = std::string(value_size, 'a');
              std::string value_read;
              while (req_done_cnt < num_requests) {
                std::string stack_name = "my_stack_" + std::to_string(dis_stack(gen));
                if (dis(gen) < push_ratio) {
                  Push(stack_name, value);
                } else {
                  Pop(stack_name, &value_read);
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
      std::cout << "*** POP only ***\n";
      // POP-only
      float push_ratio = 0.0;
      std::vector<std::thread> clients;
      auto time_begin = std::chrono::system_clock::now();
      for (int i = 0; i < num_clients; i++) {
        clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio, num_stacks] {
              std::mt19937 gen(cid);
              std::uniform_real_distribution<> dis(0.0, 1.0);
              std::uniform_int_distribution<> dis_stack(1, num_stacks);
              int req_done_cnt = 0;
              std::string value = std::string(value_size, 'a');
              std::string value_read;
              while (req_done_cnt < num_requests) {
                std::string stack_name = "my_stack_" + std::to_string(dis_stack(gen));
                if (dis(gen) < push_ratio) {
                  Push(stack_name, value);
                } else {
                  Pop(stack_name, &value_read);
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
