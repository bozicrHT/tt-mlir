// SPDX-FileCopyrightText: (c) 2024 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include "tt/runtime/ttnn/types.h"
#include "tt/runtime/ttnn/debug_apis.h"
#include "tt/runtime/ttnn/utils.h"

namespace tt::runtime::ttnn {

//
// LayoutConverter APIs
//
LayoutConverter::LayoutConverter(const LayoutDesc &inputDesc,
                                 const LayoutDesc &outputDesc)
    : inputDesc(inputDesc), outputDesc(outputDesc) {
  shouldTilize = (inputDesc.layout == ::ttnn::Layout::ROW_MAJOR &&
                  outputDesc.layout == ::ttnn::Layout::TILE);
  shouldUntilize = (inputDesc.layout == ::ttnn::Layout::TILE &&
                    outputDesc.layout == ::ttnn::Layout::ROW_MAJOR);
  shouldTypecast = (inputDesc.dataType != outputDesc.dataType);
  shouldToDevice = (inputDesc.isOnHost() && outputDesc.isOnDevice());
  shouldToMemoryConfig = (!shouldToDevice && outputDesc.isOnDevice() &&
                          (inputDesc.memoryConfig != outputDesc.memoryConfig));
  shouldFromDevice = (inputDesc.isOnDevice() && outputDesc.isOnHost());
}

::ttnn::Tensor LayoutConverter::convertTensorLayout(
    const ::ttnn::Tensor &input, std::optional<DeviceVariant> targetDevice) {
  if (inputDesc.isOnHost()) {
    return convertHostTensorLayout(input, targetDevice);
  }
  return convertDeviceTensorLayout(input);
}

::ttnn::Tensor LayoutConverter::toLayoutIfNeeded(const ::ttnn::Tensor &input) {
  if (shouldTilize) {
    return ::ttnn::to_layout(input, ::ttnn::Layout::TILE, std::nullopt,
                             std::nullopt,
                             static_cast<::ttnn::IDevice *>(nullptr));
  }
  if (shouldUntilize) {
    return ::ttnn::to_layout(input, ::ttnn::Layout::ROW_MAJOR, std::nullopt,
                             std::nullopt,
                             static_cast<::ttnn::IDevice *>(nullptr));
  }
  return input;
}

::ttnn::Tensor LayoutConverter::typecastIfNeeded(const ::ttnn::Tensor &input) {
  if (!shouldTypecast) {
    return input;
  }
  if (utils::isOnHost(input.storage_type())) {
    return ::ttnn::to_dtype(input, outputDesc.dataType);
  }
  return ::ttnn::typecast(input, outputDesc.dataType);
}

::ttnn::Tensor
LayoutConverter::toDeviceIfNeeded(const ::ttnn::Tensor &input,
                                  std::optional<DeviceVariant> targetDevice,
                                  bool force) {
  if (shouldToDevice || force) {
    LOG_ASSERT(targetDevice.has_value());
    return std::visit(
        [&](auto &&targetDevice) -> ::ttnn::Tensor {
          return ::ttnn::to_device(input, &(targetDevice.get()),
                                   outputDesc.memoryConfig);
        },
        targetDevice.value());
  }
  return input;
}

::ttnn::Tensor
LayoutConverter::toMemoryConfigIfNeeded(const ::ttnn::Tensor &input) {
  if (shouldToMemoryConfig) {
    LOG_ASSERT(outputDesc.memoryConfig.has_value());
    return ::ttnn::to_memory_config(input, outputDesc.memoryConfig.value());
  }
  return input;
}

::ttnn::Tensor
LayoutConverter::fromDeviceIfNeeded(const ::ttnn::Tensor &input) {
  if (shouldFromDevice) {
    return ::ttnn::from_device(input);
  }
  return input;
}

::ttnn::Tensor LayoutConverter::handleHostInputNoLayoutNoTypecast(
    const ::ttnn::Tensor &input, std::optional<DeviceVariant> targetDevice) {
  ::ttnn::Tensor out = toDeviceIfNeeded(input, targetDevice);
  out = toMemoryConfigIfNeeded(out);
  return out;
}

::ttnn::Tensor LayoutConverter::handleHostInputLayoutNoTypecast(
    const ::ttnn::Tensor &input, std::optional<DeviceVariant> targetDevice) {
  if (shouldUntilize) {
    ::ttnn::Tensor out = toLayoutIfNeeded(input);
    out = toDeviceIfNeeded(out, targetDevice);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }

  if (shouldTilize && outputDesc.dataType == ::ttnn::DataType::BFLOAT16) {
    ::ttnn::Tensor out = toDeviceIfNeeded(input, targetDevice);
    out = toLayoutIfNeeded(out);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }

  if (shouldTilize && outputDesc.dataType != ::ttnn::DataType::BFLOAT16) {
    ::ttnn::Tensor out = toLayoutIfNeeded(input);
    out = toDeviceIfNeeded(out, targetDevice);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }
  LOG_FATAL("Unreachable code path");
}

::ttnn::Tensor LayoutConverter::handleHostInputNoLayoutTypecast(
    const ::ttnn::Tensor &input, std::optional<DeviceVariant> targetDevice) {
  if (outputDesc.layout == ::ttnn::Layout::TILE) {
    ::ttnn::Tensor out = toDeviceIfNeeded(input, targetDevice);
    out = typecastIfNeeded(out);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }

  if (outputDesc.layout != ::ttnn::Layout::TILE) {
    ::ttnn::Tensor out = typecastIfNeeded(input);
    out = toDeviceIfNeeded(out, targetDevice);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }
  LOG_FATAL("Unreachable code path");
}

::ttnn::Tensor LayoutConverter::handleHostInputLayoutTypecast(
    const ::ttnn::Tensor &input, std::optional<DeviceVariant> targetDevice) {
  if (shouldUntilize) {
    ::ttnn::Tensor out = typecastIfNeeded(input);
    out = toLayoutIfNeeded(out);
    out = toDeviceIfNeeded(out, targetDevice);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }

  if (shouldTilize && inputDesc.dataType == ::ttnn::DataType::BFLOAT16) {
    ::ttnn::Tensor out = toDeviceIfNeeded(input, targetDevice);
    out = toLayoutIfNeeded(out);
    out = typecastIfNeeded(out);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }

  if (shouldTilize && outputDesc.dataType == ::ttnn::DataType::BFLOAT16) {
    ::ttnn::Tensor out = typecastIfNeeded(input);
    out = toDeviceIfNeeded(out, targetDevice);
    out = toLayoutIfNeeded(input);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }

  if (shouldTilize && inputDesc.dataType != ::ttnn::DataType::BFLOAT16 &&
      outputDesc.dataType != ::ttnn::DataType::BFLOAT16) {
    ::ttnn::Tensor out = typecastIfNeeded(input);
    out = toLayoutIfNeeded(out);
    out = toDeviceIfNeeded(out, targetDevice);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }

  LOG_FATAL("Unreachable code path");
}

::ttnn::Tensor LayoutConverter::convertHostTensorLayout(
    const ::ttnn::Tensor &input, std::optional<DeviceVariant> targetDevice) {
  bool shouldToLayout = (shouldTilize || shouldUntilize);
  LOG_ASSERT(!shouldToDevice || targetDevice.has_value(),
             "Target device must be provided for ToDevice");
  if (!shouldToLayout && !shouldTypecast) {
    return handleHostInputNoLayoutNoTypecast(input, targetDevice);
  }
  if (shouldToLayout && !shouldTypecast) {
    return handleHostInputLayoutNoTypecast(input, targetDevice);
  }
  if (!shouldToLayout && shouldTypecast) {
    return handleHostInputNoLayoutTypecast(input, targetDevice);
  }
  if (shouldToLayout && shouldTypecast) {
    return handleHostInputLayoutTypecast(input, targetDevice);
  }
  LOG_FATAL("Unreachable code path");
}

::ttnn::Tensor LayoutConverter::handleDeviceInputNoLayoutNoTypecast(
    const ::ttnn::Tensor &input) {
  ::ttnn::Tensor out = toMemoryConfigIfNeeded(input);
  out = fromDeviceIfNeeded(out);
  return out;
}

::ttnn::Tensor LayoutConverter::handleDeviceInputLayoutNoTypecast(
    const ::ttnn::Tensor &input) {
  if (shouldUntilize && shouldFromDevice) {
    ::ttnn::Tensor out = fromDeviceIfNeeded(input);
    out = toLayoutIfNeeded(out);
    return out;
  }

  if (shouldUntilize && !shouldFromDevice) {
    LOG_WARNING("Currently no constraint checking for on-device untilize.");
    ::ttnn::Tensor out = toLayoutIfNeeded(input);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }

  /* If we should tilize and the input data type is bfloat16, tilize on device
   */
  if (shouldTilize && inputDesc.dataType == ::ttnn::DataType::BFLOAT16) {
    ::ttnn::Tensor out = toLayoutIfNeeded(input);
    out = toMemoryConfigIfNeeded(out);
    out = fromDeviceIfNeeded(out);
    return out;
  }

  /* If we should tilize and the input data type is not bfloat16, tilize on
   * host */
  if (shouldTilize && inputDesc.dataType != ::ttnn::DataType::BFLOAT16 &&
      shouldFromDevice) {
    ::ttnn::Tensor out = fromDeviceIfNeeded(input);
    out = toLayoutIfNeeded(out);
    return out;
  }

  if (shouldTilize && inputDesc.dataType != ::ttnn::DataType::BFLOAT16 &&
      !shouldFromDevice) {
    LOG_WARNING("Currently no constraint checking for on-device tilize.");
    ::ttnn::Tensor out = toLayoutIfNeeded(input);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }

  LOG_FATAL("Unreachable code path");
}

::ttnn::Tensor LayoutConverter::handleDeviceInputNoLayoutTypecast(
    const ::ttnn::Tensor &input) {
  if (inputDesc.isTilized()) {
    ::ttnn::Tensor out = typecastIfNeeded(input);
    out = toMemoryConfigIfNeeded(out);
    out = fromDeviceIfNeeded(input);
    return out;
  }

  if (!inputDesc.isTilized() && shouldFromDevice) {
    ::ttnn::Tensor out = fromDeviceIfNeeded(input);
    out = typecastIfNeeded(out);
    return out;
  }

  if (!inputDesc.isTilized() && !shouldFromDevice) {
    LOG_WARNING("Currently no constraint checking for on-device typecast.");
    ::ttnn::Tensor out = typecastIfNeeded(input);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }
  LOG_FATAL("Unreachable code path");
}

::ttnn::Tensor
LayoutConverter::handleDeviceInputLayoutTypecast(const ::ttnn::Tensor &input) {
  if (shouldUntilize && shouldFromDevice) {
    ::ttnn::Tensor out = typecastIfNeeded(input);
    out = fromDeviceIfNeeded(input);
    out = toLayoutIfNeeded(out);
    return out;
  }

  if (shouldUntilize && !shouldFromDevice) {
    LOG_WARNING("Currently no constraint checking for on-device untilize.");
    ::ttnn::Tensor out = typecastIfNeeded(input);
    out = toLayoutIfNeeded(input);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }

  if (shouldTilize && inputDesc.dataType == ::ttnn::DataType::BFLOAT16) {
    ::ttnn::Tensor out = toLayoutIfNeeded(input);
    out = typecastIfNeeded(out);
    out = toMemoryConfigIfNeeded(out);
    out = fromDeviceIfNeeded(out);
    return out;
  }

  if (shouldTilize && inputDesc.dataType != ::ttnn::DataType::BFLOAT16 &&
      shouldFromDevice) {
    ::ttnn::Tensor out = fromDeviceIfNeeded(input);
    out = toLayoutIfNeeded(out);
    out = typecastIfNeeded(out);
    return out;
  }

  if (shouldTilize && inputDesc.dataType != ::ttnn::DataType::BFLOAT16 &&
      !shouldFromDevice) {
    LOG_WARNING("Currently no constraint checking for on-device tilize.");
    ::ttnn::Tensor out = toLayoutIfNeeded(input);
    out = typecastIfNeeded(out);
    out = toMemoryConfigIfNeeded(out);
    return out;
  }

  LOG_FATAL("Unreachable code path");
}

::ttnn::Tensor
LayoutConverter::convertDeviceTensorLayout(const ::ttnn::Tensor &input) {
  bool shouldToLayout = (shouldTilize || shouldUntilize);
  if (!shouldToLayout && !shouldTypecast) {
    return handleDeviceInputNoLayoutNoTypecast(input);
  }
  if (shouldToLayout && !shouldTypecast) {
    return handleDeviceInputLayoutNoTypecast(input);
  }
  if (!shouldToLayout && shouldTypecast) {
    return handleDeviceInputNoLayoutTypecast(input);
  }
  if (shouldToLayout && shouldTypecast) {
    return handleDeviceInputLayoutTypecast(input);
  }
  LOG_FATAL("Unreachable code path");
}

//
// ProgramTensorPool APIs
//
const ::ttnn::Tensor &ProgramTensorPool::getAndValidate(
    const ::tt::target::ttnn::TensorRef *tensorRef) const {
  LOG_ASSERT(tensorRef != nullptr, "tensorRef should not be null");
  std::uint32_t globalId = tensorRef->global_id();
  LOG_ASSERT(liveTensors.contains(globalId));
  const ::ttnn::Tensor &ttnnTensor = *liveTensors.at(globalId);
  DEBUG_ASSERT(ttnnTensor.is_allocated());
  debug::checkTensorRefMatchesTTNNTensor(tensorRef, ttnnTensor);
  return ttnnTensor;
}

::ttnn::Tensor &ProgramTensorPool::getAndValidate(
    const ::tt::target::ttnn::TensorRef *tensorRef) {
  return const_cast<::ttnn::Tensor &>(
      static_cast<const ProgramTensorPool &>(*this).getAndValidate(tensorRef));
}

std::pair<std::unordered_map<std::uint32_t, ::ttnn::Tensor *>::iterator, bool>
ProgramTensorPool::insertAndValidate(
    const ::tt::target::ttnn::TensorRef *tensorRef,
    const ::ttnn::Tensor &ttnnTensor) {
  LOG_ASSERT(tensorRef != nullptr, "tensorRef should not be null");
  std::uint32_t globalId = tensorRef->global_id();
  DEBUG_ASSERT(ttnnTensor.is_allocated());
  debug::checkTensorRefMatchesTTNNTensor(tensorRef, ttnnTensor);
  auto [iter, inserted] =
      intermedTensors.insert_or_assign(globalId, ttnnTensor);
  return liveTensors.insert_or_assign(globalId, &(iter->second));
}

size_t
ProgramTensorPool::erase(const ::tt::target::ttnn::TensorRef *tensorRef) {
  LOG_ASSERT(tensorRef != nullptr, "tensorRef should not be null");
  std::uint32_t globalId = tensorRef->global_id();
  LOG_ASSERT(liveTensors.contains(globalId) &&
             intermedTensors.contains(globalId));
  intermedTensors.erase(globalId);
  return liveTensors.erase(globalId);
}

std::vector<Tensor> ProgramTensorPool::gatherOutputTensors() {
  std::vector<Tensor> outputTensors;
  outputTensors.reserve(programOutputIds.size());
  std::transform(programOutputIds.begin(), programOutputIds.end(),
                 std::back_inserter(outputTensors),
                 [this](uint32_t outputGlobalId) -> ::tt::runtime::Tensor {
                   LOG_ASSERT(liveTensors.contains(outputGlobalId));
                   const ::ttnn::Tensor &ttnnTensor =
                       *liveTensors.at(outputGlobalId);
                   DEBUG_ASSERT(ttnnTensor.is_allocated());
                   return utils::createRuntimeTensorFromTTNN(ttnnTensor);
                 });
  return outputTensors;
}

//
// ProgramContext APIs
//
void ProgramContext::addSubMesh(uint32_t meshId,
                                std::shared_ptr<::ttnn::MeshDevice> subMesh) {
  auto [it, inserted] = subMeshes.try_emplace(meshId, subMesh);
  LOG_ASSERT(inserted, "Submesh already exists");
}

::ttnn::MeshDevice &ProgramContext::getSubMesh(uint32_t meshId) {
  LOG_ASSERT(subMeshes.contains(meshId));
  return *subMeshes.at(meshId);
}

size_t ProgramContext::subMeshSize(uint32_t meshId) const {
  LOG_ASSERT(subMeshes.contains(meshId));
  return subMeshes.at(meshId)->num_devices();
}

::ttnn::IDevice &ProgramContext::getDeviceFromSubMesh(uint32_t meshId,
                                                      int physicalDeviceId) {
  LOG_ASSERT(subMeshes.contains(meshId));
  auto &subMesh = *subMeshes.at(meshId);
  return *subMesh.get_device(physicalDeviceId);
}

::ttnn::IDevice &ProgramContext::getDeviceIndexFromSubMesh(
    uint32_t meshId, ::tt::tt_metal::distributed::MeshCoordinate meshCoords) {
  LOG_ASSERT(subMeshes.contains(meshId));
  auto &subMesh = *subMeshes.at(meshId);
  return *subMesh.get_device(meshCoords);
}

DeviceVariant ProgramContext::getTargetDevice(uint32_t meshId) {
  LOG_ASSERT(subMeshes.contains(meshId));
  auto &subMesh = *subMeshes.at(meshId);
  if (subMesh.num_devices() == 1) {
    return std::ref(
        *subMesh.get_device(::tt::tt_metal::distributed::MeshCoordinate(0, 0)));
  }
  return std::ref(subMesh);
}

} // namespace tt::runtime::ttnn
