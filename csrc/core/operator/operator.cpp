/*!
 * Copyright (c) Alibaba, Inc. and its affiliates.
 * @file    operator.cpp
 */

#include "operator.h"  // NOLINT

#include <common/engine_runtime.h>
#include <cpu/cpu_context.h>
#include <weight/weight_manager.h>

#include <fstream>
namespace allspark {

std::map<UnaryType, dnnl::algorithm> DNNLOpContext::unary_algo_map_ = {
    {UnaryType::TANH, dnnl::algorithm::eltwise_tanh},
    {UnaryType::GELU_ERF, dnnl::algorithm::eltwise_gelu_erf},
    {UnaryType::GELU_TANH, dnnl::algorithm::eltwise_gelu_tanh},
    {UnaryType::RELU, dnnl::algorithm::eltwise_relu},
    {UnaryType::SILU, dnnl::algorithm::eltwise_swish},
};
std::map<BinaryType, dnnl::algorithm> DNNLOpContext::binary_algo_map_ = {
    {BinaryType::ADD, dnnl::algorithm::binary_add},
    {BinaryType::MUL, dnnl::algorithm::binary_mul},
    {BinaryType::GEGLU,
     dnnl::algorithm::binary_mul},  // geglu is not defined in dnnl, use mul
                                    // instead
    {BinaryType::SWIGLU,
     dnnl::algorithm::binary_mul}  // swiglu is not defined in dnnl, use mul
                                   // instead
};
AsOperator::AsOperator(const std::string& op_type)
    : op_type_(op_type),
      tensor_map_(nullptr),
      ctx_(nullptr),
      gen_ctx_(nullptr) {}

void AsOperator::PrintInformation() {
  ctx_->Synchronize();
  if (ctx_->GetDeviceType() == DeviceType::CPU) {
    const CPUContext* cpu_ctx = static_cast<const CPUContext*>(ctx_);
    if (cpu_ctx->GetRank() != 0) {
      return;
    }
  }
  std::cout << string_format("{ op_name: %s, op_type: %s}", op_name_.c_str(),
                             op_type_.c_str())
            << std::endl;
  std::cout << "op_inputs:" << std::endl;
  for (auto& v : in_names_) {
    std::cout << tensor_map_->at(v)->ToString() << std::endl;
  }
  std::cout << "op_weights:" << std::endl;
  for (auto& v : weights_) {
    std::cout << v->ToString() << std::endl;
  }
  std::cout << "op_outputs:" << std::endl;
  for (auto& v : out_names_) {
    std::cout << tensor_map_->at(v)->ToString() << std::endl;
  }
  std::cout << std::endl;
}

// NOTE: File will be overrided in next step.
void AsOperator::SaveInformation() {
  ctx_->Synchronize();
  if (ctx_->GetDeviceType() == DeviceType::CPU) {
    const CPUContext* cpu_ctx = static_cast<const CPUContext*>(ctx_);
    if (cpu_ctx->GetRank() != 0) {
      return;
    }
  }
  std::ofstream OutFile(op_name_.c_str());

  OutFile << "op_inputs:" << std::endl;
  for (auto& v : in_names_) {
    OutFile << tensor_map_->at(v)->ToStringAll() << std::endl;
  }
  OutFile << "op_weights:" << std::endl;
  for (auto& v : weights_) {
    OutFile << v->ToStringAll() << std::endl;
  }
  OutFile << "op_outputs:" << std::endl;
  for (auto& v : out_names_) {
    OutFile << tensor_map_->at(v)->ToStringAll() << std::endl;
  }
  OutFile << std::endl;
}

#include <sys/stat.h>
void AsOperator::SaveTensorToBinary() {
  ctx_->Synchronize();

  std::string path = "./tmp_data/";

  struct stat st;
  if (stat(path.c_str(), &st) == -1) {
    // if the folder doesn't exist, create a folder
    if (mkdir(path.c_str(), 0777) == -1) {
      std::cerr << "Error: Failed to create folder!" << std::endl;
      return;
    } else {
      std::cout << "Success: Folder created successfully!" << std::endl;
    }
  } else {
    // if the folder exists, do nothing
    // cout << "Success: Folder already exists!" << endl;
  }

  for (auto& v : in_names_) {
    std::string filename = path + tensor_map_->at(v)->GetName();

    void* data_ptr = tensor_map_->at(v)->GetDataPtr();
    int shape_size = tensor_map_->at(v)->GetShape().Count();
    int elem_size = SizeofType(tensor_map_->at(v)->GetDataType());
    LOG(INFO) << "saving: " << filename.c_str()
              << ", shape count: " << shape_size << ", elem size: " << elem_size
              << std::endl;

    FILE* fp = fopen(filename.c_str(), "wb");
    fwrite(data_ptr, 1, shape_size * elem_size, fp);
    fclose(fp);
  }

  for (auto& v : weights_) {
    std::string filename = path + v->GetName();

    void* data_ptr = v->GetDataPtr();
    int shape_size = v->GetShape().Count();
    int elem_size = SizeofType(v->GetDataType());
    LOG(INFO) << "saving: " << filename.c_str()
              << ", shape count: " << shape_size << ", elem size: " << elem_size
              << std::endl;
    FILE* fp = fopen(filename.c_str(), "wb");
    fwrite(data_ptr, 1, shape_size * elem_size, fp);
    fclose(fp);
  }

  for (auto& v : out_names_) {
    std::string filename = path + tensor_map_->at(v)->GetName();

    void* data_ptr = tensor_map_->at(v)->GetDataPtr();
    int shape_size = tensor_map_->at(v)->GetShape().Count();
    int elem_size = SizeofType(tensor_map_->at(v)->GetDataType());
    LOG(INFO) << "saving: " << filename.c_str()
              << ", shape count: " << shape_size << ", elem size: " << elem_size
              << std::endl;

    FILE* fp = fopen(filename.c_str(), "wb");
    fwrite(data_ptr, 1, shape_size * elem_size, fp);
    fclose(fp);
  }
}

AsStatus AsOperator::Init(const OperatorProto& op_proto,
                          const DeviceContext& ctx,
                          const TensorMap& weights_map, TensorMap* tensor_map) {
  tensor_map_ = tensor_map;
  op_name_ = op_proto.op_name();
  in_names_.clear();
  for (auto& t : op_proto.inputs()) {
    const std::string& t_name = t.name();
    if (tensor_map_->count(t_name) == 0) {
      tensor_map_->insert(std::make_pair(
          t_name, std::make_unique<AsTensor>(t, ctx.GetDeviceType())));
    }
    in_names_.emplace_back(t_name);
  }
  for (auto& t : op_proto.outputs()) {
    const std::string& t_name = t.name();
    if (tensor_map_->count(t_name) == 0) {
      tensor_map_->insert(std::make_pair(
          t_name, std::make_unique<AsTensor>(t, ctx.GetDeviceType())));
    }
    out_names_.emplace_back(t_name);
  }
  for (auto& t : op_proto.weights()) {
    const std::string& t_name = t.name();
    if (weight_manager_) {
      auto weight_tensor_p =
          weight_manager_->GetWeightTensor(weight_handler_, rank_info_, t_name);
      weights_.emplace_back(weight_tensor_p.get());
    } else {
      // no manager, fallback to original weight fetch method, usually
      // it's from the unit test.
      if (weights_map.count(t_name) > 0) {
        AsTensor* weight = weights_map.at(t_name).get();
        weights_.emplace_back(weight);
      }
    }
  }
  ctx_ = &ctx;
  return AsStatus::ALLSPARK_SUCCESS;
}

AsStatus AsOperator::CallInit(
    const OperatorProto& op_proto, const DeviceContext& ctx,
    std::shared_ptr<WeightManager> weight_manager,
    std::shared_ptr<ModelWeightHandler> model_weight_handler,
    RankInfo& rankInfo, TensorMap* tensor_map, ModelProfiler* profiler) {
  profiler_ = profiler;
  weight_handler_ = model_weight_handler;
  weight_manager_ = weight_manager;
  std::string op_type = op_proto.op_type();
  auto& attr_map = op_proto.attr();
  rank_info_ = rankInfo;
  TensorMap stub_weight;  // weight already handle by weight manager, create a
                          // fake one make interface compile pass.
  return InitV2(op_proto, ctx, stub_weight, stub_weight, tensor_map);
}

AsStatus AsOperator::InitV2(const OperatorProto& op_proto,
                            const DeviceContext& ctx,
                            const TensorMap& weights_map,
                            TensorMap& weights_buffer, TensorMap* tensor_map) {
  return Init(op_proto, ctx, weights_map, tensor_map);
}

AsStatus AsOperator::SetGenerateContext(GenerateContext& gen_ctx) {
  gen_ctx_ = &gen_ctx;
  return AsStatus::ALLSPARK_SUCCESS;
}

void AsOperator::SetEmbeddingMap(std::vector<TensorListMap>* embedding_map) {
  embedding_map_ = embedding_map;
}
OpFactory& OpFactory::getInstance() {
  static OpFactory op_factory;
  return op_factory;
}

OpConstructor OpFactory::GetOperator(const OpRegistType& op_reg_type) {
  if (op_set_.find(op_reg_type) == op_set_.end()) {
    LOG(ERROR) << "Unsupported op type: " << op_reg_type.op_type_str
               << std::endl;
    throw AsException("Unsupported op type.");
  }
  return op_set_[op_reg_type];
}

void OpFactory::Register(const OpRegistType& op_reg_type,
                         OpConstructor op_constructor) {
  op_set_[op_reg_type] = op_constructor;
}

std::vector<std::string> AsOperator::GetInNames() { return in_names_; }

std::vector<std::string> AsOperator::GetOutNames() { return out_names_; }
// for debug only
TensorMap AsOperator::GetInTensors() {
  TensorMap ret;
  ctx_->Synchronize();
  for (auto& name : in_names_) {
    ret[name] = tensor_map_->at(name);
  }
  return ret;
}

// for debug only
TensorMap AsOperator::GetOutTensors() {
  TensorMap ret;
  ctx_->Synchronize();
  for (auto& name : out_names_) {
    ret[name] = tensor_map_->at(name);
  }
  return ret;
}

// for debug only
TensorMap AsOperator::GetWeights() {
  TensorMap ret;
  ctx_->Synchronize();
  for (auto& p : weights_) {
    ret[p->GetName()] = std::make_shared<AsTensor>(*p);
  }
  return ret;
}

std::string AsOperator::GetOpType() { return op_type_; }

AsStatus AsOperator::Alloc(RuntimeContext* runtime_ctx) {
  return AsStatus::ALLSPARK_SUCCESS;
}

AsStatus AsOperator::ResetCache() { return AsStatus::ALLSPARK_SUCCESS; }

AsStatus AsOperator::Reshape() { return AsStatus::ALLSPARK_SUCCESS; }

AsStatus AsOperator::Forward() { return AsStatus::ALLSPARK_SUCCESS; }

AsStatus AsOperator::Reshape(RuntimeContext* runtime_ctx) {
  return this->Reshape();
}

AsStatus AsOperator::Forward(RuntimeContext* runtime_ctx) {
  return this->Forward();
}

AsStatus AsOperator::CallForward(RuntimeContext* runtime_ctx) {
  if (profiler_) {
    ProfilerAdder adder(*profiler_, "forward", GetOpType(), ctx_);
    return Forward(runtime_ctx);
  } else {
    return Forward(runtime_ctx);
  }
}

AsStatus AsOperator::CallReshape(RuntimeContext* runtime_ctx) {
  if (profiler_) {
    ProfilerAdder adder(*profiler_, "reshape", GetOpType(), ctx_);
    return Reshape(runtime_ctx);
  } else {
    return Reshape(runtime_ctx);
  }
}

AsStatus AsOperator::CallAlloc(RuntimeContext* runtime_ctx) {
  if (profiler_) {
    ProfilerAdder adder(*profiler_, "alloc", GetOpType(), ctx_);
    return Alloc(runtime_ctx);
  } else {
    return Alloc(runtime_ctx);
  }
}

}  // namespace allspark
