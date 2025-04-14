# /ECRec/xdl/ps-plus/common/base_parity_utils.h

## 常量参数解释

### 1. `PARITY_N` 和 `PARITY_K`
```cpp
const size_t PARITY_N = 3;
const size_t PARITY_K = 2;
```
- **`PARITY_N`**：通常代表系统中总的服务器数量或者总的数据块数量。在奇偶校验相关的编码方案里，`PARITY_N` 涵盖了数据块和校验块的总数。在当前代码设定中，系统总共存在 3 个块（可能是服务器）。
- **`PARITY_K`**：表示系统里的数据块数量。也就是说，在这 3 个块中，有 2 个是实际的数据块，剩余的 `PARITY_N - PARITY_K = 1` 个则是校验块，用于数据的冗余和恢复。

### 2. `CLIENT_PARITY_FUNC`
```cpp
const std::vector<float> CLIENT_PARITY_FUNC = {1, 1, 1, 1};
```
- 这是一个浮点数向量，代表客户端使用的奇偶校验函数的系数。奇偶校验函数一般用于计算校验块的值，其系数会影响校验值的计算方式。

### 3. `INIT_BATCH_NUM_CHUNKS`
```cpp
const size_t INIT_BATCH_NUM_CHUNKS = 1 << 26;
```
- `1 << 26` 等同于 `2^26`，这个常量表示初始化批次时的块数量。在数据处理过程中，可能会把数据划分为多个块进行处理，此常量规定了初始批次所包含的块数量。

### 4. `RECOVERY_NUM_LOCKS` 和 `RECOVERY_NUM_BATCHES_PER_LOCK`
```cpp
const size_t RECOVERY_NUM_LOCKS = 10;
const size_t RECOVERY_NUM_BATCHES_PER_LOCK = 10;
```
- **`RECOVERY_NUM_LOCKS`**：表示在数据恢复过程中使用的锁的数量。锁常用于并发控制，以保证在数据恢复时不会出现冲突。
- **`RECOVERY_NUM_BATCHES_PER_LOCK`**：表示每个锁所管理的批次数量。在数据恢复时，可能会把数据分成多个批次进行处理，每个锁负责管理一定数量的批次。

### 5. `VARIABLE_NAMES_WITH_PARITY`
```cpp
const std::unordered_set<std::string> VARIABLE_NAMES_WITH_PARITY = {"emb1"};
```
- 这是一个无序集合，存储了需要进行奇偶校验的变量名。在系统中，并非所有变量都需要进行奇偶校验，这个集合明确了哪些变量需要采用奇偶校验来保证数据的可靠性。

### 6. `SIMULATED_FAILED_SERVERS` 和 `SIMULATED_RECOVERY_SERVERS`
```cpp
const std::unordered_set<size_t> SIMULATED_FAILED_SERVERS = {0};
const std::unordered_set<size_t> SIMULATED_RECOVERY_SERVERS = {0};
```
- **`SIMULATED_FAILED_SERVERS`**：这是一个无序集合，存储了模拟故障的服务器编号。在测试或者调试阶段，为了验证系统的数据恢复能力，会模拟部分服务器出现故障的情况。
- **`SIMULATED_RECOVERY_SERVERS`**：同样是一个无序集合，存储了模拟恢复的服务器编号。在模拟服务器故障后，会模拟这些服务器恢复正常，以此来测试系统的恢复机制。

### 7. `SERVER_PARITY_UPDATE`
```cpp
const bool SERVER_PARITY_UPDATE = true;
```
- 这是一个布尔类型的常量，用于控制服务器端的奇偶校验更新功能是否开启。若值为 `true`，则表示服务器端会在数据发生变化时更新奇偶校验信息；若为 `false`，则不会更新。

### 8. `HIGH_FREQ_PERCENTAGE`
```cpp
const float HIGH_FREQ_PERCENTAGE = 0.01;
```
- 这是一个浮点数，代表高频数据的百分比。在某些场景下，可能需要对高频数据进行特殊处理，这个常量规定了高频数据在总体数据中所占的比例。 

## BaseParityScheme 类构造函数解释

这段代码是 `BaseParityScheme` 类的构造函数，其主要功能是对奇偶校验方案进行初始化，设置相关的参数并计算一些必要的统计信息。以下是对代码的详细解释：

### 1. 函数声明和注释
```cpp
// currently requires parity_k <= 64
BaseParityScheme(const VariableInfo *variableInfo, size_t parity_n, size_t parity_k,
                 const std::vector<float> parity_func) {
```
- 注释 `currently requires parity_k <= 64` 表明当前代码实现要求 `parity_k` 的值不超过 64。
- 构造函数接受四个参数：
  - `variableInfo`：一个指向 `VariableInfo` 类型对象的指针，该对象包含变量的分区信息。
  - `parity_n`：表示系统中总的服务器数量或者总的数据块数量，涵盖数据块和校验块。
  - `parity_k`：表示系统里的数据块数量。
  - `parity_func`：一个浮点型向量，代表客户端使用的奇偶校验函数的系数。

### 2. 成员变量初始化
```cpp
_parity_n = parity_n;
_parity_k = parity_k;
_max_part_size = 0;
_total_size = 0u;
_num_servers = 0;
```
- 将传入的 `parity_n` 和 `parity_k` 赋值给类的成员变量 `_parity_n` 和 `_parity_k`。
- 初始化 `_max_part_size` 为 0，用于记录最大的分区大小。
- 初始化 `_total_size` 为 0，用于记录所有分区的总大小。
- 初始化 `_num_servers` 为 0，用于记录服务器的数量。

### 3. 遍历变量分区信息
```cpp
for (auto part : variableInfo->parts) {
  _total_size += part.size;
  _num_servers++;
  _max_part_size = std::max(_max_part_size, part.size);
  _server_start_ids.push_back(_total_size);
  _servers.push_back(part.server);
}
```
- 使用范围 `for` 循环遍历 `variableInfo` 中的每个分区 `part`。
  - `_total_size += part.size`：累加每个分区的大小，得到所有分区的总大小。
  - `_num_servers++`：每遍历一个分区，服务器数量加 1。
  - `_max_part_size = std::max(_max_part_size, part.size)`：更新最大分区大小。
  - `_server_start_ids.push_back(_total_size)`：将当前的总大小添加到 `_server_start_ids` 向量中，该向量记录每个服务器分区的起始位置。
  - `_servers.push_back(part.server)`：将当前分区所属的服务器编号添加到 `_servers` 向量中。

### 4. 计算单个服务器大小
```cpp
_single_server_size = ConvertClientToServerSize(_max_part_size);
```
- 调用 `ConvertClientToServerSize` 私有成员函数，将最大分区大小转换为单个服务器的大小，并将结果赋值给 `_single_server_size`。

### 5. 位图遍历和计数
```cpp
for (size_t offset_bitmap = 0; offset_bitmap < 1 << parity_n; offset_bitmap ++) {
  size_t one_count = 0;
  for (size_t j = 0; j < parity_n; j++) {
    if ((offset_bitmap & (1 << j)) > 0) one_count ++;
  }
}
```
- 外层循环遍历从 0 到 `2^parity_n - 1` 的所有位图 `offset_bitmap`。
- 内层循环遍历 `parity_n` 位，统计 `offset_bitmap` 中 1 的个数，并将结果存储在 `one_count` 中。不过，在当前代码中，`one_count` 的值没有被使用。

## MapClientToServerIds 函数解释

功能：通过并行处理，将输入张量中的每个客户端 ID 转换为对应的服务器 ID，并将结果存储在输出张量中

### 1. `tbb::blocked_range<size_t>` 简介
`TBB` 是英特尔开发的一个用于并行编程的 C++ 模板库，`tbb::blocked_range<size_t>` 是该库中的一个类模板，用于表示一个连续的整数范围，通常用于并行循环中对任务进行划分。在代码中，`tbb::parallel_for` 函数使用 `tbb::blocked_range<size_t>` 来自动将一个大的任务范围划分成多个小的子范围，然后分配给不同的线程并行处理。

### 2. `r.begin()` 和 `r.end()` 的作用
- **`r.begin()`**：该函数返回当前 `tbb::blocked_range<size_t>` 对象所表示范围的起始值。在代码的并行循环中，`r.begin()` 代表当前线程负责处理的子范围的起始索引。
- **`r.end()`**：此函数返回当前 `tbb::blocked_range<size_t>` 对象所表示范围的结束值（不包含该值）。在代码的并行循环中，`r.end()` 代表当前线程负责处理的子范围的结束索引（不包含该索引对应的元素）。

### 3. 代码示例解释
下面是包含这部分代码的片段：
```cpp
tbb::parallel_for(tbb::blocked_range<size_t>(0, num_elements), [&](tbb::blocked_range<size_t> &r) {
    for (size_t i = r.begin(); i < r.end(); i++) {
        auto client_id = *(ids.Raw<size_t>(i));
        // ... 其他代码 ...
    }
});
```
- `tbb::parallel_for` 函数将从 `0` 到 `num_elements - 1` 的范围划分成多个 `tbb::blocked_range<size_t>` 对象，并分配给不同的线程。
- 每个线程会接收到一个 `tbb::blocked_range<size_t>` 对象 `r`，该对象表示该线程需要处理的子范围。
- 线程内部的 `for` 循环从 `r.begin()` 开始，到 `r.end()` 结束（不包含 `r.end()`），依次处理该子范围内的每个元素。

## MapClientToParityIds 函数解释

功能：将客户端的 ID 映射为奇偶校验（parity）ID

## MapServerToParityIds 函数解释

功能：将服务器的 ID 映射为奇偶校验（parity）ID

## FindFriendIds 函数解释

功能：为输入张量中的每个服务器 ID 查找对应的朋友 ID，并将这些朋友 ID 存储在输出张量中

## RecoverServerValues 函数解释

功能：通过并行处理，利用已知的朋友服务器 ID 和值，调用 `RecoverSingleServerColumn` 方法恢复出目标服务器的值，并将结果存储在 result_values 张量中

## RecoverSingleServerColumn 函数解释

功能：`RecoverSingleServerColumn` 方法通过计算服务器的水平偏移量，根据朋友服务器的偏移量和值，对当前服务器的某一列的值进行恢复。





























