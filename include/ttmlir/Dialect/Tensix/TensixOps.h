// SPDX-FileCopyrightText: © 2024 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef TTMLIR_TTMLIR_DIALECT_TENSIX_TENSIXOPS_H
#define TTMLIR_TTMLIR_DIALECT_TENSIX_TENSIXOPS_H

#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dialect.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/Interfaces/ControlFlowInterfaces.h"
#include "mlir/Interfaces/DestinationStyleOpInterface.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#include "ttmlir/Dialect/Tensix/TensixOpsTypes.h"

#define GET_OP_CLASSES
#include "ttmlir/Dialect/Tensix/TensixOps.h.inc"

#endif
