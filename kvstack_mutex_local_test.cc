#include <atomic>
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
  using Value = std::pair<std::string, std::time_t>;
  using ValueInt = std::pair<uint64_t, std::time_t>;


  int Put(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lk(mu_);
    map_[key] = Value(value, std::time(nullptr));
    return 0;
  }

#if 0
  int PutInt(const std::string& key, const uint64_t value) {
    std::unique_lock<std::shared_mutex> lk(mu_);
    std::string str_val(8, ' ');
    memcpy(const_cast<char*>(str_val.data()), &value, 8);
    map_[key] = Value(str_val, std::time(nullptr));
    return 0;
  }
#else
  int PutInt(const std::string& key, const uint64_t value) {
    std::unique_lock<std::shared_mutex> lk(mu_);
    map_int_[key] = ValueInt(value, std::time(nullptr));
    return 0;
  }
#endif


  Value Get(const std::string& key) {
    std::shared_lock<std::shared_mutex> lk(mu_);
    auto it = map_.find(key);
    if (it != map_.end()) {
      return it->second;
    } else {
      return Value("", 0);
    }
  }

#if 0
  ValueInt GetInt(const std::string& key) {
    std::shared_lock<std::shared_mutex> lk(mu_);
    auto it = map_.find(key);
    if (it != map_.end()) {
      return ValueInt(*((uint64_t*) it->second.first.data()), it->second.second);
    } else {
      return ValueInt(0, 0);
    }
  }
#else
  ValueInt GetInt(const std::string& key) {
    std::shared_lock<std::shared_mutex> lk(mu_);
    auto it = map_int_.find(key);
    if (it != map_int_.end()) {
      return ValueInt(it->second.first, it->second.second);
    } else {
      return ValueInt(0, 0);
    }
  }
#endif

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

#if 0
  bool CASInt(const std::string& key, const uint64_t value_int, uint64_t& orig_int) {
    std::string value(8, ' ');
    memcpy(const_cast<char*>(value.data()), &value_int, 8);
    std::string orig(8, ' ');
    memcpy(const_cast<char*>(orig.data()), &orig_int, 8);
    std::unique_lock<std::shared_mutex> lk(mu_);
    auto it = map_.find(key);
    if (it != map_.end()) {
      if (it->second.first == orig) {
        it->second.first = value;
        it->second.second = std::time(nullptr);
        return true;
      } else {
        orig_int = *((uint64_t*) it->second.first.data());
      }
    }
    return false;
  }
#else
  bool CASInt(const std::string& key, const uint64_t value, uint64_t& orig) {
    std::unique_lock<std::shared_mutex> lk(mu_);
    auto it = map_int_.find(key);
    if (it != map_int_.end()) {
      if (it->second.first == orig) {
        it->second.first = value;
        it->second.second = std::time(nullptr);
        return true;
      } else {
        orig = it->second.first;
      }
    }
    return false;
  }
#endif

  int Load(const std::string& stack_name_key, const std::string& addr_counter_key,
           const std::string& value, const size_t num_loads) {
    uint64_t old_top_addr_num = 0;
    // Create table if not existing
    PutInt(stack_name_key, 0);
    PutInt(addr_counter_key, 0);

    for (int i = 0; i < num_loads; i++) {
      auto ret = GetInt(stack_name_key);
      old_top_addr_num = ret.first;
      // alloc counter
      uint64_t new_entry_addr_num = 0;
      ret = GetInt(addr_counter_key);
      new_entry_addr_num = ret.first + 1;
      PutInt(addr_counter_key, new_entry_addr_num);
      // new entry
      auto new_entry_addr = stack_name_key + ":@" + std::to_string(new_entry_addr_num);
      auto encoded_value = EncodeValue(value, old_top_addr_num);
      Put(new_entry_addr, encoded_value);
      while (!CASInt(stack_name_key, new_entry_addr_num, old_top_addr_num)) {
        char* p = const_cast<char*>(encoded_value.data());
        p += 2 + value.length();
        memcpy(p, &old_top_addr_num, sizeof(uint64_t));
        Put(new_entry_addr, encoded_value);
        continue;
      }
    }
    return 0;
  }

 private:
#ifdef SIMPLEKV_MAP_UNORDERED
  using Map = std::unordered_map<std::string, Value>;
  using MapInt = std::unordered_map<std::string, ValueInt>;
#else
  using Map = std::map<std::string, Value>;
  using MapInt = std::unordered_map<std::string, ValueInt>;
#endif
  std::shared_mutex mu_;
  Map map_;
  MapInt map_int_;
};

std::mutex global_mu;
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
  uint64_t val = 0;
  auto ret = kv->GetInt(addr_counter_key);
  val = ret.first;
  while (!kv->CASInt(addr_counter_key, val + 1, val)) {
    ret = kv->GetInt(addr_counter_key);
    val = ret.first;
  }
  return val + 1;
}


void PrintTop(const std::string& stack_name);
void Push(const std::string& stack_name, const std::string& value) {
  auto stack_name_key = GetStackNameKey(stack_name);
  uint64_t old_top_addr_num = 0;
  auto ret = kv->GetInt(stack_name_key);
  old_top_addr_num = ret.first;
  if (ret.second == 0) {
    std::unique_lock<std::mutex> lk(global_mu);
    ret = kv->GetInt(stack_name_key);
    if (ret.second == 0) {
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
  while (!kv->CASInt(stack_name_key, new_entry_addr_num, old_top_addr_num)) {
    ret = kv->GetInt(stack_name_key);
    old_top_addr_num = ret.first;
    char* p = const_cast<char*>(encoded_value.data());
    p += 2 + value.length();
    memcpy(p, &old_top_addr_num, sizeof(uint64_t));
    kv->Put(new_entry_addr, encoded_value);
  }
}

void PrintTop(const std::string& stack_name) {
  auto stack_name_key = GetStackNameKey(stack_name);
  uint64_t old_top_addr_num;
  auto ret = kv->GetInt(stack_name_key);
  old_top_addr_num = ret.first;
  if (ret.second == 0) {
    std::unique_lock<std::mutex> lk(global_mu);
    auto ret = kv->GetInt(stack_name_key);
    old_top_addr_num = ret.first;
    if (ret.second == 0) {
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
  auto ret1 = kv->Get(old_top_addr);
  value_read = ret1.first;
  if (ret1.second != 0) {
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
  auto ret_int = kv->GetInt(stack_name_key);
  top_addr_num = ret_int.first;
  if (ret_int.second == 0) {
    std::cout << "No stack\n";
    return;
  }
  ss << "TOP";
  uint64_t next_addr_num = top_addr_num;
  while (next_addr_num != 0) {
    auto next_top_addr = GetAddressKey(stack_name, next_addr_num);
    std::string value_read;
    auto ret = kv->Get(next_top_addr);
    value_read = ret.first;
    if (ret.second != 0) {
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
  auto stack_name_key = GetStackNameKey(stack_name);
  uint64_t old_top_addr_num = 0;
  auto top_addr_found = kv->GetInt(stack_name_key);
  old_top_addr_num = top_addr_found.first;
  SimpleKV::Value entry_found;
  while (old_top_addr_num != 0) {
//fprintf(stderr, "old_top_addr_num=%zu\n", old_top_addr_num);
    auto old_top_addr = GetAddressKey(stack_name, old_top_addr_num);
    entry_found = kv->Get(old_top_addr);
    if (entry_found.second == 0) {
      top_addr_found = kv->GetInt(stack_name_key);
      old_top_addr_num = top_addr_found.first;
      continue;
    }
    uint64_t next_top_addr_num = ExtractNextAddressNum(entry_found.first);
    if (!kv->CASInt(stack_name_key, next_top_addr_num, old_top_addr_num)) {
      top_addr_found = kv->GetInt(stack_name_key);
      old_top_addr_num = top_addr_found.first;
      continue;
    } else {
      const char* p = entry_found.first.data();
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

  printf("=========================================\n");
  printf("Benchmark configuration:\n");
  printf("  value_size: %d\n", value_size);
  printf("  num_loads: %zu\n", num_loads);
  printf("  num_requests (per client): %d", num_requests);
  printf("  num_clients: %d\n", num_clients);
  printf("  toal requests: %d\n", num_clients*num_requests);
  printf("-----------------------------------------\n");

  std::vector<double> thpts;

  {
    kv = new SimpleKV();
    {
      std::cout << "*** Load ***\n";
      // Load
      std::string stack_name = "my_stack";
      std::string value = std::string(value_size, 'a');
      kv->Load(GetStackNameKey(stack_name), GetAddressCounterKey(stack_name), value, num_loads);
    }
    {
      std::cout << "*** PUSH only ***\n";
      // PUSH-only
      float push_ratio = 1.0;
      std::vector<std::thread> clients;
      auto time_begin = std::chrono::system_clock::now();
      for (int i = 0; i < num_clients; i++) {
        clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio] {
              std::mt19937 gen(cid);
              std::uniform_real_distribution<> dis(0.0, 1.0);
              int req_done_cnt = 0;
              std::string stack_name = "my_stack";
              std::string value = std::string(value_size, 'a');
              std::string value_read;
              while (req_done_cnt < num_requests) {
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
        clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio] {
              std::mt19937 gen(cid);
              std::uniform_real_distribution<> dis(0.0, 1.0);
              int req_done_cnt = 0;
              std::string stack_name = "my_stack";
              std::string value = std::string(value_size, 'a');
              std::string value_read;
              while (req_done_cnt < num_requests) {
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
        clients.emplace_back([cid=i+1, value_size, num_requests, push_ratio] {
              std::mt19937 gen(cid);
              std::uniform_real_distribution<> dis(0.0, 1.0);
              int req_done_cnt = 0;
              std::string stack_name = "my_stack";
              std::string value = std::string(value_size, 'a');
              std::string value_read;
              while (req_done_cnt < num_requests) {
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

  return 0;
}
