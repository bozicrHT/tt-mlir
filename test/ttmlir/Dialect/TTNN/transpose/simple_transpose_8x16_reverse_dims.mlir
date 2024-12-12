// RUN: ttmlir-opt --ttir-to-ttnn-backend-pipeline %s | FileCheck %s
module attributes {} {
  func.func @forward(%arg0: tensor<64x16xbf16>) -> tensor<16x64xbf16> {
    %0 = tensor.empty() : tensor<16x64xbf16>
    // CHECK: %[[C:.*]] = "ttnn.transpose"[[C:.*]]
    %1 = "ttir.transpose"(%arg0, %0) <{dim0 = 1 : si32, dim1 = 0 : si32}> : (tensor<64x16xbf16>, tensor<16x64xbf16>) -> tensor<16x64xbf16>
    return %1 : tensor<16x64xbf16>
  }
}
