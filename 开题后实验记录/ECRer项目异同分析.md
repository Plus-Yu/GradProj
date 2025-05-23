# /ECRec/xdl/example

## /ECRec/xdl/example/criteo

新增文件夹

主要围绕 Criteo 数据集展开，包括数据的下载、处理、模型的构建和训练等功能，通过多个 Python 脚本实现了一个完整的深度学习流程

## /ECRec/xdl/example/deepctr

新增三个文件，修改`deepctr.py`一个文件

结构：
- `deepctr.py`：主要的深度学习训练脚本。
- `deepctr_checkpoint.py`：带有检查点保存功能的训练脚本。
- `deepctr_original.py`：原始的深度学习训练脚本。
- `data_generator.py`：用于生成训练数据的脚本。

# /ECRec/xdl/ps-plus

## /ECRec/xdl/ps-plus/client

这个模块的主要用途是在分布式模型训练场景下，对数据进行拆分和合并操作，以此保证数据能够正确地分布到不同的服务器，并且在需要的时候能够正确地合并

### /ECRec/xdl/ps-plus/client/partitioner

修改文件 index.cc 和 sparse.cc

通过定义一系列的分区器类和相关的上下文类，实现了数据的分区、合并以及初始化等功能，同时提供了一些归约操作的函数对象

#### `index.h` 和 `index.cc`
这两个文件定义了与索引相关的分区器类，如 `IndexDataType`、`IndexShape` 和 `IndexOffset`。

- **`IndexDataType` 类**：
  - 作用：继承自 `Partitioner`，用于按照数据类型对数据进行分区操作。
  - 关键成员函数：
    - `Split(PartitionerContext* ctx, Data* src, std::vector<Data*>* dst)`：实现了按照数据类型进行数据拆分的逻辑。

- **`IndexShape` 类**：
  - 作用：继承自 `Partitioner`，用于按照数据形状对数据进行分区操作。
  - 关键成员函数：
    - `Split(PartitionerContext* ctx, Data* src, std::vector<Data*>* dst)`：实现了按照数据形状进行数据拆分的逻辑。

- **`IndexOffset` 类**：
  - 作用：继承自 `Partitioner`，用于按照数据偏移量对数据进行分区操作。
  - 关键成员函数：
    - `Split(PartitionerContext* ctx, Data* src, std::vector<Data*>* dst)`：实现了按照数据偏移量进行数据拆分的逻辑。

#### `sparse.h` 和 `sparse.cc`
这两个文件定义了与稀疏数据相关的类和函数。

- **`HashId` 类**：
  - 作用：继承自 `HashData`，用于处理哈希ID相关的初始化操作。
  - 关键成员函数：
    - `Init(PartitionerContext* ctx, Data* src)`：初始化哈希ID。

- **`SplitOneHashId` 函数**：
  - 作用：根据哈希ID对数据进行分区，具体逻辑包括对变量信息的处理、数据形状的检查、计算哈希值、查找服务器以及生成稀疏切片等。

#### /ECRec/xdl/ps-plus/client/partitioner/index.cc

`index.cc` 功能：主要实现了三种不同的索引分区方法，分别根据数据类型、数据形状和数据偏移量对数据进行拆分，同时处理了奇偶校验的情况，确保数据在分区时能够正确适应服务器空间

在每种类的 `Split` 函数中加入了如下代码：
```cpp
  auto tmp = *info;
  if (VARIABLE_NAMES_WITH_PARITY.find(info->name) != VARIABLE_NAMES_WITH_PARITY.end()){
    BaseParityScheme pu(&tmp, PARITY_N, PARITY_K, CLIENT_PARITY_FUNC);
    pu.AdaptVariableInfoToServerSpace(&tmp);
    info = &tmp;
  }
```
这段代码的目的是检查当前变量名是否在特定集合中，如果在的话，就使用 `BaseParityScheme` 对变量信息进行调整，以适应服务器端的空间或数据布局要求

#### /ECRec/xdl/ps-plus/client/partitioner/sparse.cc

`sparse.cc` 功能：主要实现了稀疏数据的分区和组合功能，包括对稀疏 ID 和哈希 ID 的初始化，以及对稀疏数据的拆分和组合操作

### /ECRec/xdl/ps-plus/client/base_client.h

新增代码

`base_client.h` 功能：主要定义了一个名为 `BaseClient` 的基类，该基类为客户端提供了一系列与服务器进行交互的接口，用于实现各种分布式计算和数据处理相关的操作

加入了如下代码：
```cpp
    // REDUNDANCY: add sparse pull/push with parity
  virtual void IndexInitializerWithoutParity(const std::string& variable_name,
                                  Initializer* init,
                                  const Callback& cb) = 0;
  virtual void SparsePullWithoutParity(const std::string& variable_name,
                            const Tensor& ids,
                            Tensor* result,
                            const Callback& cb) = 0;
  virtual void SparsePushWithoutParity(const std::string& variable_name,
                            const Tensor& ids,
                            const std::string& updater,
                            const std::vector<Data*>& data,
                            const Callback& cb) = 0;
```
这些纯虚函数提供了在不考虑奇偶校验的情况下进行索引初始化、稀疏拉取和稀疏推送的接口

### /ECRec/xdl/ps-plus/client/client.cc

新增代码

`client.cc` 功能：主要实现了 `ps::client::Client` 类中的一系列方法，这些方法用于与服务器进行交互，完成变量的初始化、数据的拉取和推送等操作，在分布式计算或机器学习系统中可能用于客户端与服务器之间的数据通信和计算任务的处理

新增了一系列 `withParity` 相关的函数，在原项目仓库函数上增加了 `withoutParity` 的后缀。
主要在以下两个函数中增加了奇偶校验的逻辑：
```cpp
void Client::SparsePull(const std::string& variable_name,
                             const Tensor& ids,
                             Tensor* result,
                             const Callback& cb) {
  if (VARIABLE_NAMES_WITH_PARITY.find(variable_name) == VARIABLE_NAMES_WITH_PARITY.end()) {
    SparsePullWithoutParity(variable_name, ids, result, cb);
    return ;
  }
  Tensor new_ids;
  VariableInfo info;
  CHECK_ASYNC(GetVariableInfo(variable_name, &info));
  BaseParityScheme pu(&info, PARITY_N, PARITY_K, CLIENT_PARITY_FUNC);
  pu.MapClientToServerIds(ids, &new_ids);

  if (SIMULATED_FAILED_SERVERS.empty()){
    SparsePullWithoutParity(variable_name, new_ids, result, cb);
    return ;
  }

  // failed server simulation
  std::vector<size_t> ids_on_failed;
  std::vector<size_t> ids_not_on_failed;
  for (auto i = 0; i < new_ids.Shape().NumElements(); i ++) {
    auto server_id = *(new_ids.Raw<size_t>(i));
    auto server = pu.FindServer(server_id);
    if (SIMULATED_FAILED_SERVERS.find(server) == SIMULATED_FAILED_SERVERS.end()) {
      ids_not_on_failed.push_back(server_id);
    } else {
      ids_on_failed.push_back(server_id);
    }
  }

  Tensor friend_ids;
  Tensor pull_result;
  TensorShape ids_on_failed_shape({ids_on_failed.size()});
  Tensor ids_on_failed_tensor(new_ids.Type(), ids_on_failed_shape, new initializer::NoneInitializer());
  QuickMemcpy(ids_on_failed_tensor.Raw<void>(), ids_on_failed.data(), SizeOfType(ids_on_failed_tensor.Type()) * ids_on_failed.size());
  pu.FindFriendIds(ids_on_failed_tensor, &friend_ids, SIMULATED_FAILED_SERVERS);

  TensorShape sent_to_servers_shape({ids_not_on_failed.size() + friend_ids.Shape().NumElements()});
  Tensor sent_to_servers(new_ids.Type(), sent_to_servers_shape, new initializer::NoneInitializer());
  QuickMemcpy(sent_to_servers.Raw<void>(),
         friend_ids.Raw<void>(),
         SizeOfType(friend_ids.Type()) * friend_ids.Shape().NumElements());
  QuickMemcpy(sent_to_servers.Raw<size_t>(friend_ids.Shape().NumElements()),
         ids_not_on_failed.data(),
         SizeOfType(sent_to_servers.Type()) * ids_not_on_failed.size());

  std::condition_variable cv;
  std::mutex mtx;
  bool ready = false;

  auto result_cb = [&] (const Status& st) mutable {
      go(&mtx, &cv, &ready);
  };
  SparsePullWithoutParity(variable_name, sent_to_servers, &pull_result, result_cb);

  wait(&mtx, &cv, &ready);

  auto width = pull_result.Shape().Dims()[1];
  std::vector<size_t> recovered_values_shape_vec({ids_on_failed.size(), width});
  Tensor recovered_values(pull_result.Type(), TensorShape(recovered_values_shape_vec), new initializer::NoneInitializer());
  pu.RecoverServerValues(ids_on_failed_tensor, friend_ids, pull_result, &recovered_values);

  // recover target result
  std::vector<size_t> result_shape_vec({ids.Shape().NumElements(), width});
  *result = Tensor(types::kFloat, TensorShape(result_shape_vec), new initializer::NoneInitializer());
  size_t failed_index = 0;
  for (size_t i = 0; i < ids.Shape().NumElements(); i ++) {
    auto server_id = *(new_ids.Raw<size_t>(i));
    if (failed_index != ids_on_failed.size() && server_id == ids_on_failed[failed_index]) {
      QuickMemcpy(result->Raw<float>(i), recovered_values.Raw<float>(failed_index), sizeof(float) * width);
      failed_index ++;
    } else {
      auto non_failed_index = i - failed_index;
      QuickMemcpy(result->Raw<float>(i), pull_result.Raw<float>(non_failed_index), sizeof(float) * width);
    }
  }
  cb(Status::Ok());
}

void Client::SparsePush(const std::string& variable_name,
                             const Tensor& ids,
                             const std::string& updater,
                             const std::vector<Data*>& data,
                             const Callback& cb) {
  if (VARIABLE_NAMES_WITH_PARITY.find(variable_name) == VARIABLE_NAMES_WITH_PARITY.end()) {
    SparsePushWithoutParity(variable_name, ids, updater, data, cb);
    return ;
  }
  Tensor new_data_tensor;
  VariableInfo info;
  CHECK_ASYNC(GetVariableInfo(variable_name, &info));
  BaseParityScheme pu(&info, PARITY_N, PARITY_K, CLIENT_PARITY_FUNC);

  if (updater == "AssignAddUpdater" || updater == "AssignSubUpdater") {
    // case 1: assign add/sub, we can directly update parity with one round of communication
    // todo: need to redo this

  } else if (updater == "MomentumUpdater") {
    // case 2: handle momentum updater.
    // todo other updaters might also follow the same linear pattern.
    Tensor new_ids;
    auto empty_cb = [&] (const Status& st){};
    pu.MapClientToServerIds(ids, &new_ids);

    auto original_beg = data.begin();
    auto original_end = data.begin() + 4;
    std::vector<Data*> original_data(original_beg, original_end);

    if (SERVER_PARITY_UPDATE) {
      SparsePushWithoutParity(variable_name, new_ids, "MomentumServerUpdater", original_data, cb);
    } else {
      SparsePushWithoutParity(variable_name, new_ids, updater, original_data, cb);
      std::vector<Tensor> parity_ids;
      pu.MapClientToParityIds(ids, parity_ids);
      if (data.size() == 0) {
        cb(Status::ArgumentError("data length should be nonzero"));
        return;
      }

      for (size_t i = 0; i < parity_ids.size(); i ++) {
        const auto& parity_ids_tensor = parity_ids[i];
        auto par_beg = data.begin() + i * 4 + 4;
        auto par_end = data.begin() + i * 4 + 8;
        std::vector<Data*> new_data(par_beg, par_end);
        SparsePushWithoutParity(variable_name, parity_ids_tensor, updater, new_data, empty_cb);
      }

    }
  } else if (updater == "AdagradUpdater") {
    // case 2: handle momentum updater.
    // todo other updaters might also follow the same linear pattern.
    Tensor new_ids;
    auto empty_cb = [&](const Status &st) {};
    pu.MapClientToServerIds(ids, &new_ids);
    std::vector<Tensor> parity_ids;
    std::vector<Tensor> chunk_indices;
    pu.MapClientToParityIds(ids, parity_ids);
    WrapperData<std::vector<Tensor>> *data_vec_ptr =
            dynamic_cast<WrapperData<std::vector<Tensor>> *>(data[0]);
    if (data_vec_ptr == nullptr) {
      cb(Status::ArgumentError("data[0] should be tensor"));
      return;
    }
    auto data_vec = data_vec_ptr->Internal();
    auto lr_vec = dynamic_cast<WrapperData<std::vector<double>> *>(data[1])->Internal();
    auto momentum_vec = dynamic_cast<WrapperData<std::vector<double>> *>(data[2])->Internal();
    auto use_nesterov_vec = dynamic_cast<WrapperData<std::vector<bool>> *>(data[3])->Internal();

    SparsePushWithoutParity(variable_name, new_ids, "AdagradUpdaterLowPrec", data, cb);
    for (const auto &parity_ids_tensor : parity_ids) {

      std::vector<ps::Tensor> new_data_vec;
      for (size_t i = 0; i < data_vec.size(); i++) {
        auto data_tens = data_vec[i].Clone();
        size_t num_elements = ids.Shape().NumElements();
        tbb::parallel_for(tbb::blocked_range<size_t>(0, num_elements), [&](tbb::blocked_range<size_t> &r) {
            for (size_t i = r.begin(); i < r.end(); i++) {
              auto chunk_index = *(ids.Raw<size_t >(i)) % PARITY_K;
              int* bitwise_data_ptr = (int*)(data_tens.Raw<float>(i));
              (*bitwise_data_ptr) &= 0xfffffffc;
              (*bitwise_data_ptr) |= chunk_index;
            }
        });
        new_data_vec.push_back(data_tens);
      }

      auto new_data = Args(new_data_vec, lr_vec, momentum_vec, use_nesterov_vec);

      std::thread t1(&Client::SparsePushWithoutParity, this, variable_name, parity_ids_tensor,
                     "AdagradUpdater", new_data, empty_cb);
      t1.detach();
    }
  } else {
    // case 2: other operators. need to obtain diff first
    // todo not really sure if we need this
  }
}
```
`SparsePull` 新增代码通过奇偶校验方案，在模拟服务器故障的情况下，利用好友节点的数据恢复故障服务器的数据，从而保证数据的完整性。**主要步骤包括获取变量信息、模拟故障服务器、查找好友节点、合并 `ids` 进行稀疏拉取、恢复故障服务器的数据和恢复目标结果。**

`SparsePush` 新增代码根据不同的更新器类型，通过奇偶校验方案对稀疏推送操作进行处理，以确保数据的完整性和一致性。主要步骤包括初始化奇偶校验方案、根据更新器类型进行不同的处理，如映射 ids、提取数据、对奇偶校验节点进行稀疏推送等。

### /ECRec/xdl/ps-plus/client/local_client.cc

新增代码

适用于本地环境，主要处理本地服务器上的变量操作，不需要考虑分布式系统中的复杂通信和同步问题

新增的代码类似于 `client.cc`

### /ECRec/xdl/ps-plus/client/raw_client.cc

见下

### 小结

`client.h`、`raw_client.h` 和 `base_client.h` 中分别定义了 `Client`、`RawClient` 和 `BaseClient` 三个类，它们之间存在着继承和组合的关系，共同构建了一个分层的客户端架构，以下是对这三个类之间联系的详细分析：

1. 继承关系：`Client` 继承自 `BaseClient`
在 `client.h` 中，`Client` 类继承自 `BaseClient` 类，这意味着 `Client` 类是 `BaseClient` 类的具体实现，需要实现 `BaseClient` 类中定义的所有纯虚函数。
```cpp
class Client: public BaseClient {
    // ...
};
```
`BaseClient` 类是一个抽象基类，定义了客户端与分布式系统交互的基本接口，这些接口是通用的，不依赖于具体的实现细节。`Client` 类通过继承 `BaseClient` 类，确保了遵循统一的接口规范，使得上层代码可以以一致的方式调用客户端的各种功能。

2. 组合关系：`Client` 组合 `RawClient`
在 `Client` 类的构造函数中，接收一个 `RawClient` 指针作为参数，并将其存储在成员变量 `raw_` 中。这表明 `Client` 类组合了 `RawClient` 类的实例，通过委托的方式调用 `RawClient` 的方法来实现具体功能。
```cpp
class Client: public BaseClient {
public:
    Client(RawClient* raw) : raw_(raw) {}
    // ...
    std::unique_ptr<RawClient> raw_;
};
```
`RawClient` 类提供了与分布式系统交互的底层实现，包括初始化、数据处理、模型服务器交互等基本功能。`Client` 类在实现 `BaseClient` 接口时，将具体的操作委托给 `RawClient` 类的相应方法，从而实现了功能的复用和代码的解耦。

3. `RawClient` 与 `BaseClient` 的间接关联
虽然 `RawClient` 类没有直接继承 `BaseClient` 类，但它提供了与 `BaseClient` 类中定义的接口相对应的实现方法。`Client` 类在实现 `BaseClient` 接口时，通过调用 `RawClient` 的方法来完成具体的操作，因此 `RawClient` 类实际上是 `BaseClient` 接口的底层实现者。

4. 功能层次关系
- **`BaseClient` 类**：作为抽象基类，定义了客户端的通用操作接口，这些接口是与分布式系统交互的基本功能抽象，不涉及具体的实现细节。它为不同类型的客户端提供了一个统一的编程接口，使得上层代码可以以一致的方式调用客户端的各种功能。
- **`RawClient` 类**：提供了与分布式系统交互的底层实现，包括初始化、数据处理、模型服务器交互等基本功能。它是 `BaseClient` 接口的具体实现者，负责处理与分布式系统的底层通信和数据交互。
- **`Client` 类**：作为 `BaseClient` 类的具体实现，通过组合 `RawClient` 类的实例，将具体的操作委托给 `RawClient` 类的相应方法。同时，`Client` 类还可以提供一些额外的功能，如同步模式管理、参数包装等，以满足不同的应用需求。

5. 示例代码分析
以下是 `Client` 类中实现 `BaseClient` 接口的部分代码示例：
```cpp
Status Client::Init() {
    return raw_->Init();
}

void Client::Process(
    const UdfChain& udf, 
    const std::string& var_name,
    const std::vector<Data*>& datas,
    const std::vector<Partitioner*>& splitter,
    const std::vector<Partitioner*>& combiner,
    std::vector<std::unique_ptr<Data> >* results,
    const Callback& cb) override {
    return raw_->Process(udf, var_name, datas, splitter, combiner, results, cb);
}
```
在上述代码中，`Client` 类的 `Init` 方法和 `Process` 方法分别调用了 `RawClient` 类的相应方法，将具体的操作委托给 `RawClient` 类来完成。

综上所述，`Client`、`RawClient` 和 `BaseClient` 三个类之间通过继承和组合的关系，共同构建了一个分层的客户端架构，提高了代码的可维护性和可扩展性。

## /ECRec/xdl/ps-plus/common

包含了内存管理、数据初始化、性能监控、分布式协调等多个方面的功能

### /ECRec/xdl/ps-plus/common/base_parity_utils.h

新增文件

在分布式系统中实现数据冗余和恢复功能，以应对服务器故障等问题

详见另一个文件中的分析

### /ECRec/xdl/ps-plus/common/tensor.h

新增代码

`tensor.h` 功能：提供了一个完整的张量实现，支持连续和分段两种内存布局，以及多种初始化方式。通过使用该类，可以方便地创建、操作和管理多维数组。

新定义了 `Tensor` 类中的一个模板成员函数 `Multiply`，其主要功能是将张量的第一个维度乘以指定的倍数 multiplier，从而实现张量在第一个维度上的扩展。

## /ECRec/xdl/ps-plus/scheduler

### /ECRec/xdl/ps-plus/scheduler/balance_placementer_v2.cc

新增代码

`balance_placementer_v2.cc` 文件实现了一个分布式系统中的变量放置器，通过创建基础和平衡的放置方案，将变量合理地分配到服务器上，以平衡服务器之间的负载。该放置器支持密集、稀疏和哈希类型的变量，并处理了变量的内存和网络使用情况。

函数 `Placement` 根据输入的变量信息和服务器资源情况，合理地将变量分配到各个服务器上。函数通过遍历输入的变量信息，根据变量类型和参数计算切片数量、内存和网络流量，统计总网络流量和稀疏变量总内存，调用 Get 函数进行变量放置，最终生成输出的变量信息。如果放置失败，则返回参数错误状态。

新增如下代码，以正确计算内存开销
```cpp
  if (VARIABLE_NAMES_WITH_PARITY.find(x.name) == VARIABLE_NAMES_WITH_PARITY.end()) {
    total_sparse_mem += x.slice_num * x.slice_mem;
  } else {
    total_sparse_mem += x.slice_num * x.slice_mem * PARITY_N / PARITY_K + 1;
  }
```

## /ECRec/xdl/ps-plus/server

### /ECRec/xdl/ps-plus/server/checkpoint_utils.h

新增代码
























