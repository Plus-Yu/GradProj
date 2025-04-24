// REDUNDANCY: add sparse pull/push with parity
void Client::IndexInitializer(const std::string& variable_name,
                                   Initializer* init,
                                   const Callback& cb) {
  if (VARIABLE_NAMES_WITH_PARITY.find(variable_name) == VARIABLE_NAMES_WITH_PARITY.end()) {
    IndexInitializerWithoutParity(variable_name, init, cb);
    return ;
  }

  // first only initialize the variables without values
  bool init_done = false;
  auto init_cb = [&init_done](const Status& st) {
      init_done = true;
  };
  // TODO: fix this part
  IndexInitializerWithoutParity(variable_name, init, cb);
  return ;
}

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