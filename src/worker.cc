// Copyright © 2014 Wei Wang. All Rights Reserved.
// 2014-06-28 16:01
#include <glog/logging.h>

#include "worker.h"
#include "model_controller/model.h"
#include "proto/model.pb.h"
#include "proto/common.pb.h"
#include "utils/proto_helper.h"

#include "net/net.h"
#include "utils/network_thread.h"
#include "utils/global_context.h"
#include "net/sgd_trainer.h"
#include "core/table_server.h"

namespace lapis {
Worker::Worker(){
  LOG(INFO) << "starting Worker...";
  mpi_=GlobalContext::Get();
}

void SetupNet(const int batchsize,
              const char flag,
              Net *net,
              const DataSourceProtos& protos,
              const std::map<std::string, int> &store_map){
  int batchsize=proto.trainer().sgd().train_batchsize();
  auto shapes=DataSource::ShapesOf(proto.train_data());
  net->Setup(batchsize, flag, shapes, store_map);
}

bool Worker::ShouldIDoValidation(int step) {
  ShortMsg msg;
  msg.set_step(step);
  mpi_->Send(GlobalContext::kCoordinatorRank, MTYPE_VALIDATION, msg);
  mpi_->Read(GlobalContext::kCoordinatorRank, MTYPE_INSTRUCTION, &msg);
  return msg.answer();
}
void Worker::Run() {
  mc_->Init();
  TableServer *ts=nullptr;
  if(GlobalContext::Get()->AmITableServer()) {
    TableServer *ts=new TableServer();
    ts->StartTableServer();
  }

  ModelProto model;
  mpi_->Read(GlobalContext::kCoordinatorRank, MTYPE_MODEL_CONFIG, &model);
  StorageConfig sconfig;
  mpi_->Read(GlobalContext::kCoordinatorRank, MTYPE_STORAGE_CONFIG, &sconfig);

  std::map<std::string, int> train_stores=ToStdMap(sconfig.train_stores);
  std::map<std::string, int> val_stores=ToStdMap(sconfig.val_stores);
  std::map<std::string, int> test_stores=ToStdMap(sconfig.test_stores);
  std::map<int, int> tables=ToStdMap(sconfig.tables);
  mc_->CreateTables(tables);

  Net net(proto.net());
  SGDTrainer trainer;
  trainer.Init(proto.trainer(), &mc);
  const SGDProto sgd=proto.trainer().sgd();
  bool reset_net_for_training=true;
  while (!trainer.HasFinished()) {
    LOG(INFO)<<trainer.step();
    if(trainer.ValidateNow()){
      if(ShouldIDoValidation()){
        // do validation
        SetupNet(sgd.validate_batchsize(), kAllocData, &net,
            model.validation_data(), val_stores);
        trainer.Validate();
        // do test
        SetupNet(sgd.test_batchsize(), kAllocData, &net,
            model.test_data(), test_stores);
        trainer.Test();
        reset_net_for_training=true;
      }
    }
    if(reset_mpi_for_training) {
      // workers should allocate memory for data and parameters. No need to
      // Init parameters, because they Get parameters from distributed table
      SetupNet(sgd.train_batchsize(), kAllocData|kAllocParam, &net,
                model.train_data(), train_stores);
      reset_net_for_training=false;
    }
    trainer.TrainOneBatch(&net);
  }
  if(ts!=nullptr){
    ts->ShutdownTableServer();
    delete ts;
  }
}
}  // namespace lapis
