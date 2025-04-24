# 和xdl的对比区别

## 1. `index.cc` 和 `sparse.cc`

加入代码以适应纠删码的冗余空间

## 2. `client.cc` （重点）

在 pull 和 push 的逻辑中加入纠删码的逻辑，使用 lrc 时，需要改变逻辑

其他的 *_client.cc 在毕设实验中应该无需修改

## 3. `base_parity_utils.cc`（重点）

纠删码的实现，使用 lrc 时，可以仿照完成

## 4. `main.cc`

添加头文件

## 5. `balance_placementer_v2.cc`

添加代码以适应纠删码的冗余空间

## 6. `momentum_server_updater.cc`

主要关注和这个文件相关的几个文件，可能会用到修改

## 7. `checkpoint_utils.cc`（重点）

添加代码以适应纠删码的冗余空间

增加函数 LoadVariableWithRedundancy，只被 LoadVariable 调用

## 8. `server.cc`

添加了一个 no need to recover any healthy server 的逻辑

## 9. `ps_sparse_apply_momentum_op.cc`

添加了一个逻辑，可能需要关注一下

# 修改想法


















