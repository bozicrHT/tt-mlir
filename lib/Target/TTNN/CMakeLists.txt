add_mlir_translation_library(TTNNTargetFlatbuffer
    TTNNToFlatbuffer.cpp
    TTNNToFlatbufferRegistration.cpp

    ADDITIONAL_HEADER_DIRS
    ${PROJECT_SOURCE_DIR}/include/Target/TTNN

    LINK_LIBS PUBLIC
    MLIRTTNNDialect
    MLIRTTIRDialect
    MLIRTTDialect
    MLIRTTKernelDialect
    MLIRTTNNTransforms
    TTMLIRTTNNToEmitC
)

target_include_directories(TTNNTargetFlatbuffer PUBLIC ${PROJECT_BINARY_DIR}/include/ttmlir/Target/Common)

target_link_libraries(TTNNTargetFlatbuffer PUBLIC coverage_config)
