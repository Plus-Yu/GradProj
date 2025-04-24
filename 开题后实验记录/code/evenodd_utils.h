/* Copyright (C) 2016-2018 Alibaba Group Holding Limited

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// EVENODD

#ifndef PS_COMMON_EVENODD_UTILS_H_
#define PS_COMMON_EVENODD_UTILS_H_

#include <string>
#include <vector>
#include <stdlib.h>
#include <unordered_set>

#include "ps-plus/ps-plus/message/variable_info.h"
#include "ps-plus/ps-plus/common/tensor.h"
#include "ps-plus/ps-plus/common/initializer.h"
#include "ps-plus/ps-plus/common/initializer/none_initializer.h"
#include "ps-plus/ps-plus/common/types.h"
#include "tbb/parallel_for.h"

const size_t EVENODD_K = 5;
const size_t EVENODD_R = EVENODD_K - 1;
const size_t RECOVERY_NUM_LOCKS = 10;
const size_t RECOVERY_NUM_BATCHES_PER_LOCK = 10;
const std::unordered_set<std::string> VARIABLE_NAMES_WITH_PARITY = {"emb1"};
const std::unordered_set<size_t> SIMULATED_FAILED_SERVERS = {0};
const std::unordered_set<size_t> SIMULATED_RECOVERY_SERVERS = {0};
const bool SERVER_PARITY_UPDATE = true;
const float HIGH_FREQ_PERCENTAGE = 0.01; // 应该用不上

namespace ps {
class EVENODDScheme {
public:
  EVENODDScheme(const VariableInfo* variableInfo, size_t evenodd_k, size_t evenodd_r) {
    _parity_k = EVENODD_K;
    _parity_r = EVENODD_R;
    _num_servers = 0;
    _total_size = 0;
    _max_part_size = 0;

    // Initialize server information
    for (const auto& part : variableInfo->parts) {
      _total_size += part.size;
      _num_servers++;
      _max_part_size = std::max(_max_part_size, part.size);
      _server_start_ids.push_back(_total_size);
      _servers.push_back(part.server);
    }
    _single_server_size = ConvertClientToServerSize(_max_part_size);
  }

  void MapClientToServerIds(const Tensor &ids, Tensor *result_ids) {
    auto num_elements = ids.Shape().NumElements();
    *result_ids = Tensor(ids.Type(), ids.Shape(), new initializer::NoneInitializer());
    auto total_size = _single_server_size * _num_servers;
    tbb::parallel_for(tbb::blocked_range<size_t>(0, num_elements), [&](tbb::blocked_range<size_t> &r) {
        for (size_t i = r.begin(); i < r.end(); i++) {
          auto client_id = *(ids.Raw<size_t>(i));
          auto chunk_number = client_id / _parity_k;
          auto chunk_offset = client_id % _parity_k;
          auto horizontal_start = chunk_number * (_parity_k + 2);
          auto horizontal_id = horizontal_start + chunk_offset;
          auto server_id = HorizontalToVerticalId(horizontal_id);
          *(result_ids->Raw<size_t>(i)) = server_id % total_size;
        }
    });
  }

  void MapClientToParityIds(const Tensor &ids, std::vector<Tensor> &result_ids) {
    for (auto i = 0; i < 2; i++) {
      result_ids.push_back(Tensor(ids.Type(), ids.Shape(), new ps::initializer::NoneInitializer()));
    }

    auto num_elements = ids.Shape().NumElements();
    tbb::parallel_for(tbb::blocked_range<size_t>(0, num_elements), [&](tbb::blocked_range<size_t> &r) {
        for (size_t i = r.begin(); i < r.end(); i++) {
          auto client_id = *(ids.Raw<size_t>(i));
          auto chunk_number = client_id / _parity_k;
          auto chunk_index = client_id % _parity_k;
          auto horizontal_start = chunk_number * (_parity_k + 2);
          for (auto j = _parity_k; j < _parity_k + 2; j++) {
            *(result_ids[j - _parity_k].Raw<size_t>(i)) = (HorizontalToVerticalId(horizontal_start + j));
          }
        }
    });
  }

  // void MapServerToParityIds(const Tensor &ids, std::vector<Tensor> &result_ids) {
  //   for (auto i = 0; i < 2; i++) {
  //     result_ids.push_back(Tensor(ids.Type(), ids.Shape(), new ps::initializer::NoneInitializer()));
  //   }

  //   auto num_elements = ids.Shape().NumElements();
  //   tbb::parallel_for(tbb::blocked_range<size_t>(0, num_elements), [&](tbb::blocked_range<size_t> &r) {
  //       for (size_t i = r.begin(); i < r.end(); i++) {
  //         auto server_id = *(ids.Raw<size_t>(i));
  //         auto horizontal_server_id = VerticalToHorizontalId(server_id);
  //         auto client_id = _parity_k * (horizontal_server_id / _parity_n) + horizontal_server_id % _parity_n;
  //         auto chunk_number = client_id / _parity_k;
  //         auto chunk_index = client_id % _parity_k;
  //         auto horizontal_start = chunk_number * _parity_n;
  //         for (auto j = _parity_k; j < _parity_n; j++) {
  //           *(result_ids[j - _parity_k].Raw<size_t>(i)) = (HorizontalToVerticalId(horizontal_start + j));
  //         }
  //       }
  //   });
  // }

  bool FindFriendIds(const Tensor &ids, Tensor *friend_ids, std::unordered_set<size_t> failed_servers) {
    if (failed_servers.size() > 2) {
      // cant recover with too many failed servers
      return false;
    }
    // initialize result tensor
    *friend_ids = Tensor(ids.Type(), TensorShape({ids.Shape().NumElements() * _parity_k}),
                  new initializer::NoneInitializer());
    // translate
    auto num_elements = ids.Shape().NumElements();
    tbb::parallel_for(tbb::blocked_range<size_t>(0, num_elements), [&](tbb::blocked_range<size_t> &r) {
        for (size_t i = r.begin(); i < r.end(); i++) {
          auto this_id = *(ids.Raw<size_t>(i));
          std::vector<size_t> friend_ids_vector;
          MapServerIdToFriends(this_id, failed_servers, &friend_ids_vector);
          for (size_t j = 0; j < _parity_k; j++) {
            *(friend_ids->Raw<size_t>(i * _parity_k + j)) = friend_ids_vector[j];
          }
        }
    });
    return true;
  }

  void MapServerIdToFriends(size_t server_id, std::unordered_set<size_t> &failed_servers,
                            std::vector<size_t> *friend_ids) {
    auto horizontal_id = VerticalToHorizontalId(server_id);
    auto horizontal_chunk_start = horizontal_id - (horizontal_id % (_parity_k + 2));

    for (size_t offset = 0; offset < _parity_k + 2; offset++) {
      auto horizontal_friend_id = horizontal_chunk_start + offset;
      auto this_server = _servers[horizontal_id % _num_servers];
      auto friend_server = _servers[horizontal_friend_id % _num_servers];
      if (failed_servers.find(friend_server) == failed_servers.end() && friend_server != this_server) {
        // ok to add the id
        auto r = HorizontalToVerticalId(horizontal_friend_id);
        friend_ids->push_back(r);
        if (friend_ids->size() == _parity_k) break;
      }
    }
    while (friend_ids->size() < _parity_k) {
      friend_ids->push_back(server_id);
    }
  }

  // requires each entry in ids at index i correspond to its friend ids at i*k,
  // i*k+1,... i*k+k-1;
  bool RecoverServerValues(const Tensor &ids, const Tensor &friend_ids, const Tensor &friend_values,
                           Tensor *result_values, std::unordered_set<size_t> failed_servers) {
    auto num_columns = friend_values.Shape().Dims()[1];
    tbb::parallel_for(tbb::blocked_range<size_t>(0, ids.Shape().NumElements()), [&](tbb::blocked_range<size_t> &r) {
        for (size_t i = r.begin(); i < r.end(); i++) {
          RecoverSingleServerColumn(friend_ids.Raw<size_t>(i * _parity_k), *(ids.Raw<size_t>(i)),
                                               friend_values.Raw<float>(i * _parity_k), result_values->Raw<float>(i), num_columns, failed_servers);
        }
    });
    return true;
  }

  // TODO: fix recover
  bool RecoverSingleServerColumn(size_t *friend_server_indices, size_t server_index, float *friend_values,
                                 float *this_server_values, size_t num_columns, std::unordered_set<size_t> failed_servers) {
    auto server_offset = VerticalToHorizontalId(server_index) % (_parity_k + 2);
    if (failed_servers.size() == 1) {
      if (server_offset <= _parity_k) {
        for (size_t col = 0; col < num_columns; col++) {
          this_server_values[col] = 0.0;
          uint32_t this_value = *reinterpret_cast<const uint32_t*>(&this_server_values[col]);
          for (size_t i = 0; i < _parity_k; i++) {
            auto friend_server_offset = VerticalToHorizontalId(friend_server_indices[i]) % (_parity_k + 2);
            if (friend_server_offset <= _parity_k) {
              uint32_t value = *reinterpret_cast<const uint32_t*>(&friend_values[col * _parity_k + i]);
              this_value ^= value;
            }
          }
          this_server_values[col] = *reinterpret_cast<const float*>(&this_value);
        }
      } else {
        for (size_t col = 0; col < num_columns; col++) {
          uint8_t s;
          get_s(col, friend_server_indices, friend_values, &s);
          get_q(col, friend_server_indices, friend_values, s, this_server_values);
        }
      }
    } else if (failed_servers.size() == 2) {
      // TODO: two faults
      bool p_flag = false, q_flag = false;
      for (size_t i = 0; i < _parity_k; i++) {
        auto friend_server_offset = VerticalToHorizontalId(friend_server_indices[i]) % (_parity_k + 2);
        if (friend_server_offset == _parity_k) {
          p_flag = true;
        } else if (friend_server_offset == _parity_k + 1) {
          q_flag = true;
        }
      }
      if (server_offset < _parity_k && !p_flag && !q_flag) {
        for (size_t col = 0; col < num_columns; col++) {
          uint8_t s;
          get_s(col, friend_server_indices, friend_values, &s);
          // TODO
        }
      } else if (server_offset < _parity_k && p_flag && !q_flag) {

      } else if (server_offset < _parity_k && !p_flag && q_flag) {
        
      } else if (server_offset == _parity_k && p_flag && q_flag) {
        
      } else if (server_offset == _parity_k + 1 && p_flag && q_flag) {
        
      }
    }

    return true;
  }

  size_t FindServer(size_t server_id) {
    auto horizontal_id = VerticalToHorizontalId(server_id);
    return _servers[horizontal_id % _num_servers];
  }

  void AdaptVariableInfoToServerSpace(VariableInfo *info) {
    info->shape[0] = _single_server_size * _num_servers;
    for (size_t i = 0; i < info->parts.size(); i ++) {
      info->parts[i].size = _single_server_size;
    }
  }

private:
  size_t HorizontalToVerticalId(size_t horizontal_id) {
    auto server = horizontal_id % _num_servers;
    auto offset = horizontal_id / _num_servers;
    return offset + server * _single_server_size;
  }

  size_t VerticalToHorizontalId(size_t vertical_id) {
    auto server = vertical_id / _single_server_size;
    auto offset = vertical_id % _single_server_size;
    return server + offset * _num_servers;
  }

  size_t ConvertClientToServerSize(size_t si) {
    if (si * (_parity_k + 2) % _parity_k == 0) return si * (_parity_k + 2) / _parity_k;
    else return si * (_parity_k + 2) / _parity_k + 1;
  }

  void get_s(size_t col, size_t *friend_server_indices, float *friend_values, uint8_t *s) {
    float *f = new float[_parity_k];
    for (size_t i = 0; i < _parity_k; i++) {
      auto friend_server_offset = VerticalToHorizontalId(friend_server_indices[i]) % (_parity_k + 2);
      if (friend_server_offset <= _parity_k) {
        f[i] = friend_values[col * _parity_k + i];
      }
    }
    uint8_t *bytes = new uint8_t[_parity_k * 4];
    SplitFloatToBytes(f, 5, bytes);
    delete[] f;

    *s = 0;
    for (size_t i = 0; i < 4; i++)
      *s ^= bytes[5 * i + 4];
    delete[] bytes;
  }

  void get_s_1(size_t col, size_t *friend_server_indices, float *friend_values, uint8_t *s) {
    float f1, f2;
    for (size_t i = 0; i < _parity_k; i++) {
      auto friend_server_offset = VerticalToHorizontalId(friend_server_indices[i]) % (_parity_k + 2);
      if (friend_server_offset == _parity_k) {
        f1 = friend_values[col * _parity_k + i];
      } else if (friend_server_offset == _parity_k + 1) {
        f2 = friend_values[col * _parity_k + i];
      }
    }
    uint32_t value1 = *reinterpret_cast<const uint32_t*>(&f1);
    uint32_t value2 = *reinterpret_cast<const uint32_t*>(&f2);
    value1 ^= value2;

    uint8_t *bytes = new uint8_t[4];
    bytes[0] = (value1 >> 0) & 0xFF;  // 提取第 1 个字节
    bytes[1] = (value1 >> 8) & 0xFF;  // 提取第 2 个字节
    bytes[2] = (value1 >> 16) & 0xFF; // 提取第 3 个字节
    bytes[3] = (value1 >> 24) & 0xFF; // 提取第 4 个字节

    *s = 0;
    for (size_t i = 0; i < 4; i++)
      *s ^= bytes[i];
    delete[] bytes;
  }

  void get_q(size_t col, size_t *friend_server_indices, float *friend_values, const uint8_t s, float *q) {
    float *f = new float[_parity_k];
    for (size_t i = 0; i < _parity_k; i++) {
      auto friend_server_offset = VerticalToHorizontalId(friend_server_indices[i]) % (_parity_k + 2);
      if (friend_server_offset <= _parity_k) {
        f[i] = friend_values[col * _parity_k + i];
      }
    }
    uint8_t *bytes = new uint8_t[_parity_k * 4];
    SplitFloatToBytes(f, 5, bytes);
    delete[] f;

    uint8_t tmp[4];
    for (int i = 0; i < 4; i++) {
      tmp[i] = s ^ bytes[i] ^ bytes[5 + i] ^ bytes[10 + i] ^ bytes[15 + i];
    }
    uint32_t value = 0;
    value |= (bytes[0] << 0);
    value |= (bytes[1] << 8);
    value |= (bytes[2] << 16);
    value |= (bytes[3] << 24);

    q[col] = *reinterpret_cast<float*>(&value);
  }

  void SplitFloatToBytes(const float* floats, size_t count, uint8_t* bytes) {
    for (size_t i = 0; i < count; ++i) {
        uint32_t value = *reinterpret_cast<const uint32_t*>(&floats[i]); // 将 float 转换为 uint32_t
        bytes[i * 4 + 0] = (value >> 0) & 0xFF;  // 提取第 1 个字节
        bytes[i * 4 + 1] = (value >> 8) & 0xFF;  // 提取第 2 个字节
        bytes[i * 4 + 2] = (value >> 16) & 0xFF; // 提取第 3 个字节
        bytes[i * 4 + 3] = (value >> 24) & 0xFF; // 提取第 4 个字节
    }
  }

  std::vector<size_t> _server_start_ids;
  size_t _max_part_size;
  size_t _single_server_size;
  size_t _num_servers;
  size_t _total_size;
  size_t _parity_k;
  size_t _parity_r;
  std::vector<size_t> _servers;
};
} // namespace ps

#endif // PS_COMMON_EVENODD_UTILS_H_



















