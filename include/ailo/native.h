/* Generated from the pinned upstream public API inventory.
 * Regenerate with scripts/generate_native.py; do not edit manually.
 * See docs/facade-bindings.md for ownership and bridge semantics. */
#ifndef AILO_NATIVE_H
#define AILO_NATIVE_H
#include "ailo/base.h"
#include "ailo/native_macros.h"
#include "ailo/native_variadic.h"
#include <stddef.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
struct ailo_ggml_backend;
struct ailo_ggml_backend_buffer;
struct ailo_ggml_backend_buffer_type;
struct ailo_ggml_backend_dev_caps;
struct ailo_ggml_backend_dev_props;
struct ailo_ggml_backend_device;
struct ailo_ggml_backend_event;
struct ailo_ggml_backend_feature;
struct ailo_ggml_backend_graph_copy;
struct ailo_ggml_backend_meta_split_state;
struct ailo_ggml_backend_reg;
struct ailo_ggml_backend_sched;
struct ailo_ggml_bf16_t;
struct ailo_ggml_cgraph;
struct ailo_ggml_context;
struct ailo_ggml_cplan;
struct ailo_ggml_gallocr;
struct ailo_ggml_init_params;
struct ailo_ggml_object;
struct ailo_ggml_opt_context;
struct ailo_ggml_opt_dataset;
struct ailo_ggml_opt_optimizer_params;
struct ailo_ggml_opt_params;
struct ailo_ggml_opt_result;
struct ailo_ggml_tallocr;
struct ailo_ggml_tensor;
struct ailo_ggml_threadpool;
struct ailo_ggml_threadpool_params;
struct ailo_ggml_type_traits;
struct ailo_ggml_type_traits_cpu;
struct ailo_gguf_context;
struct ailo_gguf_init_params;
struct ailo_llama_adapter_lora;
struct ailo_llama_batch;
struct ailo_llama_batch_ext;
struct ailo_llama_chat_message;
struct ailo_llama_context;
struct ailo_llama_context_params;
struct ailo_llama_embd;
struct ailo_llama_logit_bias;
struct ailo_llama_memory_i;
struct ailo_llama_model;
struct ailo_llama_model_imatrix_data;
struct ailo_llama_model_kv_override;
struct ailo_llama_model_params;
struct ailo_llama_model_quantize_params;
struct ailo_llama_model_tensor_buft_override;
struct ailo_llama_model_tensor_override;
struct ailo_llama_opt_params;
struct ailo_llama_perf_context_data;
struct ailo_llama_perf_sampler_data;
struct ailo_llama_sampler;
struct ailo_llama_sampler_chain_params;
struct ailo_llama_sampler_data;
struct ailo_llama_sampler_i;
struct ailo_llama_sampler_seq_config;
struct ailo_llama_token_data;
struct ailo_llama_token_data_array;
struct ailo_llama_vocab;
struct ailo_mtmd_batch;
struct ailo_mtmd_bitmap;
struct ailo_mtmd_caps;
struct ailo_mtmd_context;
struct ailo_mtmd_context_params;
struct ailo_mtmd_decoder_pos;
struct ailo_mtmd_gen_audio_info;
struct ailo_mtmd_gen_inp;
struct ailo_mtmd_gen_out;
struct ailo_mtmd_helper_bitmap_wrapper;
struct ailo_mtmd_helper_embd_batch;
struct ailo_mtmd_helper_gen_audio;
struct ailo_mtmd_helper_gen_audio_inp;
struct ailo_mtmd_helper_init_opt;
struct ailo_mtmd_helper_video;
struct ailo_mtmd_helper_video_info;
struct ailo_mtmd_helper_video_init_params;
struct ailo_mtmd_image_tokens;
struct ailo_mtmd_input_chunk;
struct ailo_mtmd_input_chunks;
struct ailo_mtmd_input_part;
struct ailo_mtmd_input_text;
struct ailo_mtmd_memory_usage;
struct ailo_storage_int64_t;
struct ailo_storage_uint64_t;
struct ailo_storage_bool;
struct ailo_storage_struct_ggml_context;
struct ailo_storage_struct_ggml_backend_device;
struct ailo_storage_struct_ggml_tensor;
struct ailo_storage_struct_ggml_backend;
struct ailo_storage_struct_ggml_backend_buffer_type;
struct ailo_storage_struct_llama_adapter_lora;
struct ailo_storage_struct_mtmd_bitmap;
struct ailo_storage_const_struct_mtmd_bitmap;
struct ailo_storage_const_struct_mtmd_input_part;
struct ailo_backend_memory_map;
enum ailo_ggml_backend_buffer_usage {
  AILO_GGML_BACKEND_BUFFER_USAGE_ANY = 0,
  AILO_GGML_BACKEND_BUFFER_USAGE_WEIGHTS = 1,
  AILO_GGML_BACKEND_BUFFER_USAGE_COMPUTE = 2
};
enum ailo_ggml_backend_dev_type {
  AILO_GGML_BACKEND_DEVICE_TYPE_CPU = 0,
  AILO_GGML_BACKEND_DEVICE_TYPE_GPU = 1,
  AILO_GGML_BACKEND_DEVICE_TYPE_IGPU = 2,
  AILO_GGML_BACKEND_DEVICE_TYPE_ACCEL = 3,
  AILO_GGML_BACKEND_DEVICE_TYPE_META = 4
};
enum ailo_ggml_backend_meta_split_axis {
  AILO_GGML_BACKEND_SPLIT_AXIS_0 = 0,
  AILO_GGML_BACKEND_SPLIT_AXIS_1 = 1,
  AILO_GGML_BACKEND_SPLIT_AXIS_2 = 2,
  AILO_GGML_BACKEND_SPLIT_AXIS_3 = 3,
  AILO_GGML_BACKEND_SPLIT_AXIS_MIRRORED = 10,
  AILO_GGML_BACKEND_SPLIT_AXIS_PARTIAL = 11,
  AILO_GGML_BACKEND_SPLIT_AXIS_NONE = 98,
  AILO_GGML_BACKEND_SPLIT_AXIS_UNKNOWN = 99
};
enum ailo_ggml_ftype {
  AILO_GGML_FTYPE_UNKNOWN = -1,
  AILO_GGML_FTYPE_ALL_F32 = 0,
  AILO_GGML_FTYPE_MOSTLY_F16 = 1,
  AILO_GGML_FTYPE_MOSTLY_Q4_0 = 2,
  AILO_GGML_FTYPE_MOSTLY_Q4_1 = 3,
  AILO_GGML_FTYPE_MOSTLY_Q4_1_SOME_F16 = 4,
  AILO_GGML_FTYPE_MOSTLY_Q8_0 = 7,
  AILO_GGML_FTYPE_MOSTLY_Q5_0 = 8,
  AILO_GGML_FTYPE_MOSTLY_Q5_1 = 9,
  AILO_GGML_FTYPE_MOSTLY_Q2_K = 10,
  AILO_GGML_FTYPE_MOSTLY_Q3_K = 11,
  AILO_GGML_FTYPE_MOSTLY_Q4_K = 12,
  AILO_GGML_FTYPE_MOSTLY_Q5_K = 13,
  AILO_GGML_FTYPE_MOSTLY_Q6_K = 14,
  AILO_GGML_FTYPE_MOSTLY_IQ2_XXS = 15,
  AILO_GGML_FTYPE_MOSTLY_IQ2_XS = 16,
  AILO_GGML_FTYPE_MOSTLY_IQ3_XXS = 17,
  AILO_GGML_FTYPE_MOSTLY_IQ1_S = 18,
  AILO_GGML_FTYPE_MOSTLY_IQ4_NL = 19,
  AILO_GGML_FTYPE_MOSTLY_IQ3_S = 20,
  AILO_GGML_FTYPE_MOSTLY_IQ2_S = 21,
  AILO_GGML_FTYPE_MOSTLY_IQ4_XS = 22,
  AILO_GGML_FTYPE_MOSTLY_IQ1_M = 23,
  AILO_GGML_FTYPE_MOSTLY_BF16 = 24,
  AILO_GGML_FTYPE_MOSTLY_MXFP4 = 25,
  AILO_GGML_FTYPE_MOSTLY_NVFP4 = 26,
  AILO_GGML_FTYPE_MOSTLY_Q1_0 = 27,
  AILO_GGML_FTYPE_MOSTLY_Q2_0 = 28
};
enum ailo_ggml_glu_op {
  AILO_GGML_GLU_OP_REGLU = 0,
  AILO_GGML_GLU_OP_GEGLU = 1,
  AILO_GGML_GLU_OP_SWIGLU = 2,
  AILO_GGML_GLU_OP_SWIGLU_OAI = 3,
  AILO_GGML_GLU_OP_GEGLU_ERF = 4,
  AILO_GGML_GLU_OP_GEGLU_QUICK = 5,
  AILO_GGML_GLU_OP_SWIGLU_CLAMP = 6,
  AILO_GGML_GLU_OP_COUNT = 7
};
enum ailo_ggml_log_level {
  AILO_GGML_LOG_LEVEL_NONE = 0,
  AILO_GGML_LOG_LEVEL_DEBUG = 1,
  AILO_GGML_LOG_LEVEL_INFO = 2,
  AILO_GGML_LOG_LEVEL_WARN = 3,
  AILO_GGML_LOG_LEVEL_ERROR = 4,
  AILO_GGML_LOG_LEVEL_CONT = 5
};
enum ailo_ggml_numa_strategy {
  AILO_GGML_NUMA_STRATEGY_DISABLED = 0,
  AILO_GGML_NUMA_STRATEGY_DISTRIBUTE = 1,
  AILO_GGML_NUMA_STRATEGY_ISOLATE = 2,
  AILO_GGML_NUMA_STRATEGY_NUMACTL = 3,
  AILO_GGML_NUMA_STRATEGY_MIRROR = 4,
  AILO_GGML_NUMA_STRATEGY_COUNT = 5
};
enum ailo_ggml_object_type {
  AILO_GGML_OBJECT_TYPE_TENSOR = 0,
  AILO_GGML_OBJECT_TYPE_GRAPH = 1,
  AILO_GGML_OBJECT_TYPE_WORK_BUFFER = 2
};
enum ailo_ggml_op {
  AILO_GGML_OP_NONE = 0,
  AILO_GGML_OP_DUP = 1,
  AILO_GGML_OP_ADD = 2,
  AILO_GGML_OP_ADD_ID = 3,
  AILO_GGML_OP_ADD1 = 4,
  AILO_GGML_OP_ACC = 5,
  AILO_GGML_OP_SUB = 6,
  AILO_GGML_OP_MUL = 7,
  AILO_GGML_OP_DIV = 8,
  AILO_GGML_OP_SQR = 9,
  AILO_GGML_OP_SQRT = 10,
  AILO_GGML_OP_LOG = 11,
  AILO_GGML_OP_SIN = 12,
  AILO_GGML_OP_COS = 13,
  AILO_GGML_OP_SUM = 14,
  AILO_GGML_OP_SUM_ROWS = 15,
  AILO_GGML_OP_CUMSUM = 16,
  AILO_GGML_OP_MEAN = 17,
  AILO_GGML_OP_ARGMAX = 18,
  AILO_GGML_OP_COUNT_EQUAL = 19,
  AILO_GGML_OP_REPEAT = 20,
  AILO_GGML_OP_REPEAT_BACK = 21,
  AILO_GGML_OP_CONCAT = 22,
  AILO_GGML_OP_SILU_BACK = 23,
  AILO_GGML_OP_NORM = 24,
  AILO_GGML_OP_RMS_NORM = 25,
  AILO_GGML_OP_RMS_NORM_BACK = 26,
  AILO_GGML_OP_GROUP_NORM = 27,
  AILO_GGML_OP_L2_NORM = 28,
  AILO_GGML_OP_MUL_MAT = 29,
  AILO_GGML_OP_MUL_MAT_ID = 30,
  AILO_GGML_OP_OUT_PROD = 31,
  AILO_GGML_OP_SCALE = 32,
  AILO_GGML_OP_SET = 33,
  AILO_GGML_OP_CPY = 34,
  AILO_GGML_OP_CONT = 35,
  AILO_GGML_OP_RESHAPE = 36,
  AILO_GGML_OP_VIEW = 37,
  AILO_GGML_OP_PERMUTE = 38,
  AILO_GGML_OP_TRANSPOSE = 39,
  AILO_GGML_OP_GET_ROWS = 40,
  AILO_GGML_OP_GET_ROWS_BACK = 41,
  AILO_GGML_OP_SET_ROWS = 42,
  AILO_GGML_OP_DIAG = 43,
  AILO_GGML_OP_DIAG_MASK_INF = 44,
  AILO_GGML_OP_DIAG_MASK_ZERO = 45,
  AILO_GGML_OP_SOFT_MAX = 46,
  AILO_GGML_OP_SOFT_MAX_BACK = 47,
  AILO_GGML_OP_ROPE = 48,
  AILO_GGML_OP_ROPE_BACK = 49,
  AILO_GGML_OP_CLAMP = 50,
  AILO_GGML_OP_CONV_TRANSPOSE_1D = 51,
  AILO_GGML_OP_IM2COL = 52,
  AILO_GGML_OP_IM2COL_BACK = 53,
  AILO_GGML_OP_IM2COL_3D = 54,
  AILO_GGML_OP_COL2IM_1D = 55,
  AILO_GGML_OP_CONV_2D = 56,
  AILO_GGML_OP_CONV_3D = 57,
  AILO_GGML_OP_CONV_2D_DW = 58,
  AILO_GGML_OP_CONV_TRANSPOSE_2D = 59,
  AILO_GGML_OP_POOL_1D = 60,
  AILO_GGML_OP_POOL_2D = 61,
  AILO_GGML_OP_POOL_2D_BACK = 62,
  AILO_GGML_OP_UPSCALE = 63,
  AILO_GGML_OP_PAD = 64,
  AILO_GGML_OP_PAD_REFLECT_1D = 65,
  AILO_GGML_OP_ROLL = 66,
  AILO_GGML_OP_ARANGE = 67,
  AILO_GGML_OP_TIMESTEP_EMBEDDING = 68,
  AILO_GGML_OP_ARGSORT = 69,
  AILO_GGML_OP_TOP_K = 70,
  AILO_GGML_OP_LEAKY_RELU = 71,
  AILO_GGML_OP_TRI = 72,
  AILO_GGML_OP_FILL = 73,
  AILO_GGML_OP_FLASH_ATTN_EXT = 74,
  AILO_GGML_OP_FLASH_ATTN_BACK = 75,
  AILO_GGML_OP_SSM_CONV = 76,
  AILO_GGML_OP_SSM_SCAN = 77,
  AILO_GGML_OP_WIN_PART = 78,
  AILO_GGML_OP_WIN_UNPART = 79,
  AILO_GGML_OP_GET_REL_POS = 80,
  AILO_GGML_OP_ADD_REL_POS = 81,
  AILO_GGML_OP_RWKV_WKV6 = 82,
  AILO_GGML_OP_GATED_LINEAR_ATTN = 83,
  AILO_GGML_OP_RWKV_WKV7 = 84,
  AILO_GGML_OP_SOLVE_TRI = 85,
  AILO_GGML_OP_GATED_DELTA_NET = 86,
  AILO_GGML_OP_LIGHTNING_INDEXER = 87,
  AILO_GGML_OP_DSV4_HC_COMB = 88,
  AILO_GGML_OP_DSV4_HC_PRE = 89,
  AILO_GGML_OP_DSV4_HC_POST = 90,
  AILO_GGML_OP_UNARY = 91,
  AILO_GGML_OP_MAP_CUSTOM1 = 92,
  AILO_GGML_OP_MAP_CUSTOM2 = 93,
  AILO_GGML_OP_MAP_CUSTOM3 = 94,
  AILO_GGML_OP_CUSTOM = 95,
  AILO_GGML_OP_CROSS_ENTROPY_LOSS = 96,
  AILO_GGML_OP_CROSS_ENTROPY_LOSS_BACK = 97,
  AILO_GGML_OP_OPT_STEP_ADAMW = 98,
  AILO_GGML_OP_OPT_STEP_SGD = 99,
  AILO_GGML_OP_GLU = 100,
  AILO_GGML_OP_COUNT = 101
};
enum ailo_ggml_op_hint {
  AILO_GGML_HINT_NONE = 0,
  AILO_GGML_HINT_SRC0_IS_HADAMARD = 1
};
enum ailo_ggml_op_pool {
  AILO_GGML_OP_POOL_MAX = 0,
  AILO_GGML_OP_POOL_AVG = 1,
  AILO_GGML_OP_POOL_COUNT = 2
};
enum ailo_ggml_opt_build_type {
  AILO_GGML_OPT_BUILD_TYPE_FORWARD = 10,
  AILO_GGML_OPT_BUILD_TYPE_GRAD = 20,
  AILO_GGML_OPT_BUILD_TYPE_OPT = 30
};
enum ailo_ggml_opt_loss_type {
  AILO_GGML_OPT_LOSS_TYPE_MEAN = 0,
  AILO_GGML_OPT_LOSS_TYPE_SUM = 1,
  AILO_GGML_OPT_LOSS_TYPE_CROSS_ENTROPY = 2,
  AILO_GGML_OPT_LOSS_TYPE_MEAN_SQUARED_ERROR = 3
};
enum ailo_ggml_opt_optimizer_type {
  AILO_GGML_OPT_OPTIMIZER_TYPE_ADAMW = 0,
  AILO_GGML_OPT_OPTIMIZER_TYPE_SGD = 1,
  AILO_GGML_OPT_OPTIMIZER_TYPE_COUNT = 2
};
enum ailo_ggml_prec {
  AILO_GGML_PREC_UNDEFINED = 0,
  AILO_GGML_PREC_DEFAULT = 0,
  AILO_GGML_PREC_F32 = 10,
  AILO_GGML_PREC_BF16 = 15,
  AILO_GGML_PREC_F16 = 20,
  AILO_GGML_PREC_Q8 = 30,
  AILO_GGML_PREC_Q4 = 40
};
enum ailo_ggml_scale_flag {
  AILO_GGML_SCALE_FLAG_ALIGN_CORNERS = 256,
  AILO_GGML_SCALE_FLAG_ANTIALIAS = 512
};
enum ailo_ggml_scale_mode {
  AILO_GGML_SCALE_MODE_NEAREST = 0,
  AILO_GGML_SCALE_MODE_BILINEAR = 1,
  AILO_GGML_SCALE_MODE_BICUBIC = 2,
  AILO_GGML_SCALE_MODE_COUNT = 3
};
enum ailo_ggml_sched_priority {
  AILO_GGML_SCHED_PRIO_LOW = -1,
  AILO_GGML_SCHED_PRIO_NORMAL = 0,
  AILO_GGML_SCHED_PRIO_MEDIUM = 1,
  AILO_GGML_SCHED_PRIO_HIGH = 2,
  AILO_GGML_SCHED_PRIO_REALTIME = 3
};
enum ailo_ggml_sort_order {
  AILO_GGML_SORT_ORDER_ASC = 0,
  AILO_GGML_SORT_ORDER_DESC = 1
};
enum ailo_ggml_status {
  AILO_GGML_STATUS_ALLOC_FAILED = -2,
  AILO_GGML_STATUS_FAILED = -1,
  AILO_GGML_STATUS_SUCCESS = 0,
  AILO_GGML_STATUS_ABORTED = 1
};
enum ailo_ggml_tensor_flag {
  AILO_GGML_TENSOR_FLAG_INPUT = 1,
  AILO_GGML_TENSOR_FLAG_OUTPUT = 2,
  AILO_GGML_TENSOR_FLAG_PARAM = 4,
  AILO_GGML_TENSOR_FLAG_LOSS = 8,
  AILO_GGML_TENSOR_FLAG_COMPUTE = 16
};
enum ailo_ggml_tri_type {
  AILO_GGML_TRI_TYPE_UPPER_DIAG = 0,
  AILO_GGML_TRI_TYPE_UPPER = 1,
  AILO_GGML_TRI_TYPE_LOWER_DIAG = 2,
  AILO_GGML_TRI_TYPE_LOWER = 3
};
enum ailo_ggml_type {
  AILO_GGML_TYPE_F32 = 0,
  AILO_GGML_TYPE_F16 = 1,
  AILO_GGML_TYPE_Q4_0 = 2,
  AILO_GGML_TYPE_Q4_1 = 3,
  AILO_GGML_TYPE_Q5_0 = 6,
  AILO_GGML_TYPE_Q5_1 = 7,
  AILO_GGML_TYPE_Q8_0 = 8,
  AILO_GGML_TYPE_Q8_1 = 9,
  AILO_GGML_TYPE_Q2_K = 10,
  AILO_GGML_TYPE_Q3_K = 11,
  AILO_GGML_TYPE_Q4_K = 12,
  AILO_GGML_TYPE_Q5_K = 13,
  AILO_GGML_TYPE_Q6_K = 14,
  AILO_GGML_TYPE_Q8_K = 15,
  AILO_GGML_TYPE_IQ2_XXS = 16,
  AILO_GGML_TYPE_IQ2_XS = 17,
  AILO_GGML_TYPE_IQ3_XXS = 18,
  AILO_GGML_TYPE_IQ1_S = 19,
  AILO_GGML_TYPE_IQ4_NL = 20,
  AILO_GGML_TYPE_IQ3_S = 21,
  AILO_GGML_TYPE_IQ2_S = 22,
  AILO_GGML_TYPE_IQ4_XS = 23,
  AILO_GGML_TYPE_I8 = 24,
  AILO_GGML_TYPE_I16 = 25,
  AILO_GGML_TYPE_I32 = 26,
  AILO_GGML_TYPE_I64 = 27,
  AILO_GGML_TYPE_F64 = 28,
  AILO_GGML_TYPE_IQ1_M = 29,
  AILO_GGML_TYPE_BF16 = 30,
  AILO_GGML_TYPE_TQ1_0 = 34,
  AILO_GGML_TYPE_TQ2_0 = 35,
  AILO_GGML_TYPE_MXFP4 = 39,
  AILO_GGML_TYPE_NVFP4 = 40,
  AILO_GGML_TYPE_Q1_0 = 41,
  AILO_GGML_TYPE_Q2_0 = 42,
  AILO_GGML_TYPE_COUNT = 43
};
enum ailo_ggml_unary_op {
  AILO_GGML_UNARY_OP_ABS = 0,
  AILO_GGML_UNARY_OP_SGN = 1,
  AILO_GGML_UNARY_OP_NEG = 2,
  AILO_GGML_UNARY_OP_STEP = 3,
  AILO_GGML_UNARY_OP_TANH = 4,
  AILO_GGML_UNARY_OP_ELU = 5,
  AILO_GGML_UNARY_OP_RELU = 6,
  AILO_GGML_UNARY_OP_SIGMOID = 7,
  AILO_GGML_UNARY_OP_GELU = 8,
  AILO_GGML_UNARY_OP_GELU_QUICK = 9,
  AILO_GGML_UNARY_OP_SILU = 10,
  AILO_GGML_UNARY_OP_HARDSWISH = 11,
  AILO_GGML_UNARY_OP_HARDSIGMOID = 12,
  AILO_GGML_UNARY_OP_EXP = 13,
  AILO_GGML_UNARY_OP_EXPM1 = 14,
  AILO_GGML_UNARY_OP_SOFTPLUS = 15,
  AILO_GGML_UNARY_OP_GELU_ERF = 16,
  AILO_GGML_UNARY_OP_XIELU = 17,
  AILO_GGML_UNARY_OP_FLOOR = 18,
  AILO_GGML_UNARY_OP_CEIL = 19,
  AILO_GGML_UNARY_OP_ROUND = 20,
  AILO_GGML_UNARY_OP_TRUNC = 21,
  AILO_GGML_UNARY_OP_COUNT = 22
};
enum ailo_gguf_type {
  AILO_GGUF_TYPE_UINT8 = 0,
  AILO_GGUF_TYPE_INT8 = 1,
  AILO_GGUF_TYPE_UINT16 = 2,
  AILO_GGUF_TYPE_INT16 = 3,
  AILO_GGUF_TYPE_UINT32 = 4,
  AILO_GGUF_TYPE_INT32 = 5,
  AILO_GGUF_TYPE_FLOAT32 = 6,
  AILO_GGUF_TYPE_BOOL = 7,
  AILO_GGUF_TYPE_STRING = 8,
  AILO_GGUF_TYPE_ARRAY = 9,
  AILO_GGUF_TYPE_UINT64 = 10,
  AILO_GGUF_TYPE_INT64 = 11,
  AILO_GGUF_TYPE_FLOAT64 = 12,
  AILO_GGUF_TYPE_COUNT = 13
};
enum ailo_llama_attention_type {
  AILO_LLAMA_ATTENTION_TYPE_UNSPECIFIED = -1,
  AILO_LLAMA_ATTENTION_TYPE_CAUSAL = 0,
  AILO_LLAMA_ATTENTION_TYPE_NON_CAUSAL = 1
};
enum ailo_llama_context_type {
  AILO_LLAMA_CONTEXT_TYPE_DEFAULT = 0,
  AILO_LLAMA_CONTEXT_TYPE_MTP = 1
};
enum ailo_llama_flash_attn_type {
  AILO_LLAMA_FLASH_ATTN_TYPE_AUTO = -1,
  AILO_LLAMA_FLASH_ATTN_TYPE_DISABLED = 0,
  AILO_LLAMA_FLASH_ATTN_TYPE_ENABLED = 1
};
enum ailo_llama_ftype {
  AILO_LLAMA_FTYPE_ALL_F32 = 0,
  AILO_LLAMA_FTYPE_MOSTLY_F16 = 1,
  AILO_LLAMA_FTYPE_MOSTLY_Q4_0 = 2,
  AILO_LLAMA_FTYPE_MOSTLY_Q4_1 = 3,
  AILO_LLAMA_FTYPE_MOSTLY_Q8_0 = 7,
  AILO_LLAMA_FTYPE_MOSTLY_Q5_0 = 8,
  AILO_LLAMA_FTYPE_MOSTLY_Q5_1 = 9,
  AILO_LLAMA_FTYPE_MOSTLY_Q2_K = 10,
  AILO_LLAMA_FTYPE_MOSTLY_Q3_K_S = 11,
  AILO_LLAMA_FTYPE_MOSTLY_Q3_K_M = 12,
  AILO_LLAMA_FTYPE_MOSTLY_Q3_K_L = 13,
  AILO_LLAMA_FTYPE_MOSTLY_Q4_K_S = 14,
  AILO_LLAMA_FTYPE_MOSTLY_Q4_K_M = 15,
  AILO_LLAMA_FTYPE_MOSTLY_Q5_K_S = 16,
  AILO_LLAMA_FTYPE_MOSTLY_Q5_K_M = 17,
  AILO_LLAMA_FTYPE_MOSTLY_Q6_K = 18,
  AILO_LLAMA_FTYPE_MOSTLY_IQ2_XXS = 19,
  AILO_LLAMA_FTYPE_MOSTLY_IQ2_XS = 20,
  AILO_LLAMA_FTYPE_MOSTLY_Q2_K_S = 21,
  AILO_LLAMA_FTYPE_MOSTLY_IQ3_XS = 22,
  AILO_LLAMA_FTYPE_MOSTLY_IQ3_XXS = 23,
  AILO_LLAMA_FTYPE_MOSTLY_IQ1_S = 24,
  AILO_LLAMA_FTYPE_MOSTLY_IQ4_NL = 25,
  AILO_LLAMA_FTYPE_MOSTLY_IQ3_S = 26,
  AILO_LLAMA_FTYPE_MOSTLY_IQ3_M = 27,
  AILO_LLAMA_FTYPE_MOSTLY_IQ2_S = 28,
  AILO_LLAMA_FTYPE_MOSTLY_IQ2_M = 29,
  AILO_LLAMA_FTYPE_MOSTLY_IQ4_XS = 30,
  AILO_LLAMA_FTYPE_MOSTLY_IQ1_M = 31,
  AILO_LLAMA_FTYPE_MOSTLY_BF16 = 32,
  AILO_LLAMA_FTYPE_MOSTLY_TQ1_0 = 36,
  AILO_LLAMA_FTYPE_MOSTLY_TQ2_0 = 37,
  AILO_LLAMA_FTYPE_MOSTLY_MXFP4_MOE = 38,
  AILO_LLAMA_FTYPE_MOSTLY_NVFP4 = 39,
  AILO_LLAMA_FTYPE_MOSTLY_Q1_0 = 40,
  AILO_LLAMA_FTYPE_MOSTLY_Q2_0 = 41,
  AILO_LLAMA_FTYPE_GUESSED = 1024
};
enum ailo_llama_lazy_mode {
  AILO_LLAMA_LAZY_MODE_OFF = 0,
  AILO_LLAMA_LAZY_MODE_AUTO = 1,
  AILO_LLAMA_LAZY_MODE_ON = 2
};
enum ailo_llama_load_mode {
  AILO_LLAMA_LOAD_MODE_AUTO = -1,
  AILO_LLAMA_LOAD_MODE_NONE = 0,
  AILO_LLAMA_LOAD_MODE_MMAP = 1,
  AILO_LLAMA_LOAD_MODE_MLOCK = 2,
  AILO_LLAMA_LOAD_MODE_MMAP_MLOCK = 3,
  AILO_LLAMA_LOAD_MODE_DIRECT_IO = 4
};
enum ailo_llama_model_kv_override_type {
  AILO_LLAMA_KV_OVERRIDE_TYPE_INT = 0,
  AILO_LLAMA_KV_OVERRIDE_TYPE_FLOAT = 1,
  AILO_LLAMA_KV_OVERRIDE_TYPE_BOOL = 2,
  AILO_LLAMA_KV_OVERRIDE_TYPE_STR = 3
};
enum ailo_llama_model_meta_key {
  AILO_LLAMA_MODEL_META_KEY_SAMPLING_SEQUENCE = 0,
  AILO_LLAMA_MODEL_META_KEY_SAMPLING_TOP_K = 1,
  AILO_LLAMA_MODEL_META_KEY_SAMPLING_TOP_P = 2,
  AILO_LLAMA_MODEL_META_KEY_SAMPLING_MIN_P = 3,
  AILO_LLAMA_MODEL_META_KEY_SAMPLING_XTC_PROBABILITY = 4,
  AILO_LLAMA_MODEL_META_KEY_SAMPLING_XTC_THRESHOLD = 5,
  AILO_LLAMA_MODEL_META_KEY_SAMPLING_TEMP = 6,
  AILO_LLAMA_MODEL_META_KEY_SAMPLING_PENALTY_LAST_N = 7,
  AILO_LLAMA_MODEL_META_KEY_SAMPLING_PENALTY_REPEAT = 8,
  AILO_LLAMA_MODEL_META_KEY_SAMPLING_MIROSTAT = 9,
  AILO_LLAMA_MODEL_META_KEY_SAMPLING_MIROSTAT_TAU = 10,
  AILO_LLAMA_MODEL_META_KEY_SAMPLING_MIROSTAT_ETA = 11
};
enum ailo_llama_pooling_type {
  AILO_LLAMA_POOLING_TYPE_UNSPECIFIED = -1,
  AILO_LLAMA_POOLING_TYPE_NONE = 0,
  AILO_LLAMA_POOLING_TYPE_MEAN = 1,
  AILO_LLAMA_POOLING_TYPE_CLS = 2,
  AILO_LLAMA_POOLING_TYPE_LAST = 3,
  AILO_LLAMA_POOLING_TYPE_RANK = 4
};
enum ailo_llama_process_type {
  AILO_LLAMA_PROCESS_TYPE_ENCODE = 0,
  AILO_LLAMA_PROCESS_TYPE_DECODE = 1
};
enum ailo_llama_rope_scaling_type {
  AILO_LLAMA_ROPE_SCALING_TYPE_UNSPECIFIED = -1,
  AILO_LLAMA_ROPE_SCALING_TYPE_NONE = 0,
  AILO_LLAMA_ROPE_SCALING_TYPE_LINEAR = 1,
  AILO_LLAMA_ROPE_SCALING_TYPE_YARN = 2,
  AILO_LLAMA_ROPE_SCALING_TYPE_LONGROPE = 3,
  AILO_LLAMA_ROPE_SCALING_TYPE_MAX_VALUE = 3
};
enum ailo_llama_rope_type {
  AILO_LLAMA_ROPE_TYPE_NONE = -1,
  AILO_LLAMA_ROPE_TYPE_NORM = 0,
  AILO_LLAMA_ROPE_TYPE_NEOX = 2,
  AILO_LLAMA_ROPE_TYPE_MROPE = 8,
  AILO_LLAMA_ROPE_TYPE_IMROPE = 40,
  AILO_LLAMA_ROPE_TYPE_VISION = 24
};
enum ailo_llama_split_mode {
  AILO_LLAMA_SPLIT_MODE_NONE = 0,
  AILO_LLAMA_SPLIT_MODE_LAYER = 1,
  AILO_LLAMA_SPLIT_MODE_ROW = 2,
  AILO_LLAMA_SPLIT_MODE_TENSOR = 3
};
enum ailo_llama_token_attr {
  AILO_LLAMA_TOKEN_ATTR_UNDEFINED = 0,
  AILO_LLAMA_TOKEN_ATTR_UNKNOWN = 1,
  AILO_LLAMA_TOKEN_ATTR_UNUSED = 2,
  AILO_LLAMA_TOKEN_ATTR_NORMAL = 4,
  AILO_LLAMA_TOKEN_ATTR_CONTROL = 8,
  AILO_LLAMA_TOKEN_ATTR_USER_DEFINED = 16,
  AILO_LLAMA_TOKEN_ATTR_BYTE = 32,
  AILO_LLAMA_TOKEN_ATTR_NORMALIZED = 64,
  AILO_LLAMA_TOKEN_ATTR_LSTRIP = 128,
  AILO_LLAMA_TOKEN_ATTR_RSTRIP = 256,
  AILO_LLAMA_TOKEN_ATTR_SINGLE_WORD = 512
};
enum ailo_llama_token_type {
  AILO_LLAMA_TOKEN_TYPE_UNDEFINED = 0,
  AILO_LLAMA_TOKEN_TYPE_NORMAL = 1,
  AILO_LLAMA_TOKEN_TYPE_UNKNOWN = 2,
  AILO_LLAMA_TOKEN_TYPE_CONTROL = 3,
  AILO_LLAMA_TOKEN_TYPE_USER_DEFINED = 4,
  AILO_LLAMA_TOKEN_TYPE_UNUSED = 5,
  AILO_LLAMA_TOKEN_TYPE_BYTE = 6
};
enum ailo_llama_vocab_type {
  AILO_LLAMA_VOCAB_TYPE_NONE = 0,
  AILO_LLAMA_VOCAB_TYPE_SPM = 1,
  AILO_LLAMA_VOCAB_TYPE_BPE = 2,
  AILO_LLAMA_VOCAB_TYPE_WPM = 3,
  AILO_LLAMA_VOCAB_TYPE_UGM = 4,
  AILO_LLAMA_VOCAB_TYPE_RWKV = 5,
  AILO_LLAMA_VOCAB_TYPE_PLAMO2 = 6,
  AILO_LLAMA_VOCAB_TYPE_TEST = 7
};
enum ailo_mtmd_gen_audio_type {
  AILO_MTMD_GEN_AUDIO_TYPE_NONE = 0,
  AILO_MTMD_GEN_AUDIO_TYPE_QWEN3TTS = 1,
  AILO_MTMD_GEN_AUDIO_TYPE_POCKETTTS = 2
};
enum ailo_mtmd_gen_process_type {
  AILO_MTMD_GEN_PROCESS_TYPE_GEN_CODE = 0,
  AILO_MTMD_GEN_PROCESS_TYPE_GEN_WAV = 1
};
enum ailo_mtmd_helper_gen_audio_outtype {
  AILO_MTMD_HELPER_GEN_AUDIO_OUTTYPE_PCM = 0,
  AILO_MTMD_HELPER_GEN_AUDIO_OUTTYPE_WAV = 1
};
enum ailo_mtmd_input_chunk_type {
  AILO_MTMD_INPUT_CHUNK_TYPE_TEXT = 0,
  AILO_MTMD_INPUT_CHUNK_TYPE_IMAGE = 1,
  AILO_MTMD_INPUT_CHUNK_TYPE_AUDIO = 2,
  AILO_MTMD_INPUT_CHUNK_TYPE_COUNT = 3
};
typedef struct ailo_ggml_backend_buffer *ailo_ggml_backend_buffer_t;
typedef struct ailo_ggml_backend_buffer_type *ailo_ggml_backend_buffer_type_t;
typedef struct ailo_ggml_backend_device *ailo_ggml_backend_dev_t;
typedef struct ailo_ggml_backend_event *ailo_ggml_backend_event_t;
typedef void *ailo_ggml_backend_graph_plan_t;
typedef struct ailo_ggml_backend_reg *ailo_ggml_backend_reg_t;
typedef struct ailo_ggml_backend_sched *ailo_ggml_backend_sched_t;
typedef struct ailo_ggml_backend *ailo_ggml_backend_t;
typedef struct ailo_ggml_bf16_t ailo_ggml_bf16_t;
typedef ailo_u16 ailo_ggml_fp16_t;
typedef struct ailo_ggml_gallocr *ailo_ggml_gallocr_t;
typedef ailo_u8 ailo_ggml_guid[16];
typedef ailo_ggml_guid *ailo_ggml_guid_t;
typedef struct ailo_ggml_opt_context *ailo_ggml_opt_context_t;
typedef struct ailo_ggml_opt_dataset *ailo_ggml_opt_dataset_t;
typedef struct ailo_ggml_opt_result *ailo_ggml_opt_result_t;
typedef struct ailo_ggml_threadpool *ailo_ggml_threadpool_t;
typedef struct ailo_llama_batch ailo_llama_batch;
typedef struct ailo_llama_chat_message ailo_llama_chat_message;
typedef struct ailo_llama_logit_bias ailo_llama_logit_bias;
typedef struct ailo_llama_memory_i *ailo_llama_memory_t;
typedef struct ailo_llama_model_quantize_params
    ailo_llama_model_quantize_params;
typedef ailo_i32 ailo_llama_pos;
typedef struct ailo_llama_sampler_chain_params ailo_llama_sampler_chain_params;
typedef void *ailo_llama_sampler_context_t;
typedef ailo_i32 ailo_llama_seq_id;
typedef ailo_u32 ailo_llama_state_seq_flags;
typedef ailo_i32 ailo_llama_token;
typedef struct ailo_llama_token_data ailo_llama_token_data;
typedef struct ailo_llama_token_data_array ailo_llama_token_data_array;
typedef struct ailo_mtmd_batch ailo_mtmd_batch;
typedef struct ailo_mtmd_bitmap ailo_mtmd_bitmap;
typedef struct ailo_mtmd_context ailo_mtmd_context;
typedef struct ailo_mtmd_helper_gen_audio ailo_mtmd_helper_gen_audio;
typedef struct ailo_mtmd_helper_init_opt ailo_mtmd_helper_init_opt;
typedef struct ailo_mtmd_helper_video ailo_mtmd_helper_video;
typedef struct ailo_mtmd_image_tokens ailo_mtmd_image_tokens;
typedef struct ailo_mtmd_input_chunk ailo_mtmd_input_chunk;
typedef struct ailo_mtmd_input_chunks ailo_mtmd_input_chunks;
typedef struct ailo_mtmd_input_part ailo_mtmd_input_part;
typedef struct ailo_mtmd_input_text ailo_mtmd_input_text;
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status
ailo_ggml_backend_dev_caps_array_new(size_t count,
                                     struct ailo_ggml_backend_dev_caps **output,
                                     ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_ggml_backend_dev_caps_array_free(struct ailo_ggml_backend_dev_caps *self,
                                      ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_backend_dev_caps_array_at(
    struct ailo_ggml_backend_dev_caps *self, size_t count, size_t index,
    struct ailo_ggml_backend_dev_caps **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_dev_caps_async_get(
    const struct ailo_ggml_backend_dev_caps *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_backend_dev_caps_async_set(struct ailo_ggml_backend_dev_caps *self,
                                     ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_dev_caps_host_buffer_get(
    const struct ailo_ggml_backend_dev_caps *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_dev_caps_host_buffer_set(
    struct ailo_ggml_backend_dev_caps *self, ailo_bool value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_dev_caps_buffer_from_host_ptr_get(
    const struct ailo_ggml_backend_dev_caps *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_dev_caps_buffer_from_host_ptr_set(
    struct ailo_ggml_backend_dev_caps *self, ailo_bool value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_dev_caps_events_get(
    const struct ailo_ggml_backend_dev_caps *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_backend_dev_caps_events_set(struct ailo_ggml_backend_dev_caps *self,
                                      ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_dev_caps_mmap_support_get(
    const struct ailo_ggml_backend_dev_caps *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_dev_caps_mmap_support_set(
    struct ailo_ggml_backend_dev_caps *self, ailo_bool value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_backend_dev_props_array_new(
    size_t count, struct ailo_ggml_backend_dev_props **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_ggml_backend_dev_props_array_free(struct ailo_ggml_backend_dev_props *self,
                                       ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_backend_dev_props_array_at(
    struct ailo_ggml_backend_dev_props *self, size_t count, size_t index,
    struct ailo_ggml_backend_dev_props **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_dev_props_name_get(
    const struct ailo_ggml_backend_dev_props *self, const char **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_backend_dev_props_name_set(struct ailo_ggml_backend_dev_props *self,
                                     const char *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_dev_props_description_get(
    const struct ailo_ggml_backend_dev_props *self, const char **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_dev_props_description_set(
    struct ailo_ggml_backend_dev_props *self, const char *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_dev_props_memory_free_get(
    const struct ailo_ggml_backend_dev_props *self, size_t *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_dev_props_memory_free_set(
    struct ailo_ggml_backend_dev_props *self, size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_dev_props_memory_total_get(
    const struct ailo_ggml_backend_dev_props *self, size_t *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_dev_props_memory_total_set(
    struct ailo_ggml_backend_dev_props *self, size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_dev_props_type_get(
    const struct ailo_ggml_backend_dev_props *self,
    enum ailo_ggml_backend_dev_type *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_backend_dev_props_type_set(struct ailo_ggml_backend_dev_props *self,
                                     enum ailo_ggml_backend_dev_type value,
                                     ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_dev_props_device_id_get(
    const struct ailo_ggml_backend_dev_props *self, const char **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_dev_props_device_id_set(
    struct ailo_ggml_backend_dev_props *self, const char *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_dev_props_caps_get(
    const struct ailo_ggml_backend_dev_props *self,
    struct ailo_ggml_backend_dev_caps **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_dev_props_caps_set(
    struct ailo_ggml_backend_dev_props *self,
    const struct ailo_ggml_backend_dev_caps *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_backend_feature_array_new(
    size_t count, struct ailo_ggml_backend_feature **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_ggml_backend_feature_array_free(struct ailo_ggml_backend_feature *self,
                                     ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_backend_feature_array_at(
    struct ailo_ggml_backend_feature *self, size_t count, size_t index,
    struct ailo_ggml_backend_feature **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_backend_feature_name_get(const struct ailo_ggml_backend_feature *self,
                                   const char **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_backend_feature_name_set(struct ailo_ggml_backend_feature *self,
                                   const char *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_feature_value_get(
    const struct ailo_ggml_backend_feature *self, const char **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_backend_feature_value_set(struct ailo_ggml_backend_feature *self,
                                    const char *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_backend_graph_copy_array_new(
    size_t count, struct ailo_ggml_backend_graph_copy **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_ggml_backend_graph_copy_array_free(
    struct ailo_ggml_backend_graph_copy *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_backend_graph_copy_array_at(
    struct ailo_ggml_backend_graph_copy *self, size_t count, size_t index,
    struct ailo_ggml_backend_graph_copy **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_graph_copy_buffer_get(
    const struct ailo_ggml_backend_graph_copy *self,
    struct ailo_ggml_backend_buffer **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_graph_copy_buffer_set(
    struct ailo_ggml_backend_graph_copy *self,
    struct ailo_ggml_backend_buffer *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_graph_copy_ctx_allocated_get(
    const struct ailo_ggml_backend_graph_copy *self,
    struct ailo_ggml_context **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_graph_copy_ctx_allocated_set(
    struct ailo_ggml_backend_graph_copy *self, struct ailo_ggml_context *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_graph_copy_ctx_unallocated_get(
    const struct ailo_ggml_backend_graph_copy *self,
    struct ailo_ggml_context **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_graph_copy_ctx_unallocated_set(
    struct ailo_ggml_backend_graph_copy *self, struct ailo_ggml_context *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_graph_copy_graph_get(
    const struct ailo_ggml_backend_graph_copy *self,
    struct ailo_ggml_cgraph **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_graph_copy_graph_set(
    struct ailo_ggml_backend_graph_copy *self, struct ailo_ggml_cgraph *value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_backend_meta_split_state_array_new(
    size_t count, struct ailo_ggml_backend_meta_split_state **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_ggml_backend_meta_split_state_array_free(
    struct ailo_ggml_backend_meta_split_state *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_backend_meta_split_state_array_at(
    struct ailo_ggml_backend_meta_split_state *self, size_t count, size_t index,
    struct ailo_ggml_backend_meta_split_state **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_meta_split_state_axis_get(
    const struct ailo_ggml_backend_meta_split_state *self,
    enum ailo_ggml_backend_meta_split_axis *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_meta_split_state_axis_set(
    struct ailo_ggml_backend_meta_split_state *self,
    enum ailo_ggml_backend_meta_split_axis value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_meta_split_state_ne_get(
    const struct ailo_ggml_backend_meta_split_state *self, size_t index,
    ailo_i64 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_meta_split_state_ne_set(
    struct ailo_ggml_backend_meta_split_state *self, size_t index,
    ailo_i64 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_meta_split_state_nr_get(
    const struct ailo_ggml_backend_meta_split_state *self, size_t index,
    ailo_u32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_meta_split_state_nr_set(
    struct ailo_ggml_backend_meta_split_state *self, size_t index,
    ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_backend_meta_split_state_n_segments_get(
    const struct ailo_ggml_backend_meta_split_state *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_backend_meta_split_state_n_segments_set(
    struct ailo_ggml_backend_meta_split_state *self, ailo_u32 value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_cplan_array_new(size_t count,
                                      struct ailo_ggml_cplan **output,
                                      ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_ggml_cplan_array_free(struct ailo_ggml_cplan *self,
                                       ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_cplan_array_at(struct ailo_ggml_cplan *self, size_t count,
                                     size_t index,
                                     struct ailo_ggml_cplan **output,
                                     ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_cplan_work_size_get(const struct ailo_ggml_cplan *self,
                                          size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_cplan_work_size_set(struct ailo_ggml_cplan *self,
                                          size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_cplan_work_data_get(const struct ailo_ggml_cplan *self,
                                          ailo_u8 **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_cplan_work_data_set(struct ailo_ggml_cplan *self,
                                          ailo_u8 *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_cplan_n_threads_get(const struct ailo_ggml_cplan *self,
                                          int *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_cplan_n_threads_set(struct ailo_ggml_cplan *self,
                                          int value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_cplan_threadpool_get(const struct ailo_ggml_cplan *self,
                                           struct ailo_ggml_threadpool **output,
                                           ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_cplan_threadpool_set(struct ailo_ggml_cplan *self,
                                           struct ailo_ggml_threadpool *value,
                                           ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_cplan_abort_callback_data_get(const struct ailo_ggml_cplan *self,
                                        void **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_cplan_abort_callback_data_set(struct ailo_ggml_cplan *self,
                                        void *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_cplan_use_ref_get(const struct ailo_ggml_cplan *self,
                                        ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_cplan_use_ref_set(struct ailo_ggml_cplan *self,
                                        ailo_bool value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_init_params_array_new(
    size_t count, struct ailo_ggml_init_params **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_ggml_init_params_array_free(struct ailo_ggml_init_params *self,
                                             ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_init_params_array_at(
    struct ailo_ggml_init_params *self, size_t count, size_t index,
    struct ailo_ggml_init_params **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_init_params_mem_size_get(const struct ailo_ggml_init_params *self,
                                   size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_init_params_mem_size_set(struct ailo_ggml_init_params *self,
                                   size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_init_params_mem_buffer_get(const struct ailo_ggml_init_params *self,
                                     void **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_init_params_mem_buffer_set(struct ailo_ggml_init_params *self,
                                     void *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_init_params_no_alloc_get(const struct ailo_ggml_init_params *self,
                                   ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_init_params_no_alloc_set(struct ailo_ggml_init_params *self,
                                   ailo_bool value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_opt_optimizer_params_array_new(
    size_t count, struct ailo_ggml_opt_optimizer_params **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_ggml_opt_optimizer_params_array_free(
    struct ailo_ggml_opt_optimizer_params *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_opt_optimizer_params_array_at(
    struct ailo_ggml_opt_optimizer_params *self, size_t count, size_t index,
    struct ailo_ggml_opt_optimizer_params **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_opt_optimizer_params_adamw_alpha_get(
    const struct ailo_ggml_opt_optimizer_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_opt_optimizer_params_adamw_alpha_set(
    struct ailo_ggml_opt_optimizer_params *self, float value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_opt_optimizer_params_adamw_beta1_get(
    const struct ailo_ggml_opt_optimizer_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_opt_optimizer_params_adamw_beta1_set(
    struct ailo_ggml_opt_optimizer_params *self, float value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_opt_optimizer_params_adamw_beta2_get(
    const struct ailo_ggml_opt_optimizer_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_opt_optimizer_params_adamw_beta2_set(
    struct ailo_ggml_opt_optimizer_params *self, float value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_opt_optimizer_params_adamw_eps_get(
    const struct ailo_ggml_opt_optimizer_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_opt_optimizer_params_adamw_eps_set(
    struct ailo_ggml_opt_optimizer_params *self, float value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_opt_optimizer_params_adamw_wd_get(
    const struct ailo_ggml_opt_optimizer_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_opt_optimizer_params_adamw_wd_set(
    struct ailo_ggml_opt_optimizer_params *self, float value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_opt_optimizer_params_sgd_alpha_get(
    const struct ailo_ggml_opt_optimizer_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_opt_optimizer_params_sgd_alpha_set(
    struct ailo_ggml_opt_optimizer_params *self, float value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_opt_optimizer_params_sgd_wd_get(
    const struct ailo_ggml_opt_optimizer_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_opt_optimizer_params_sgd_wd_set(
    struct ailo_ggml_opt_optimizer_params *self, float value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_opt_params_array_new(size_t count,
                                           struct ailo_ggml_opt_params **output,
                                           ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_ggml_opt_params_array_free(struct ailo_ggml_opt_params *self,
                                            ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_opt_params_array_at(struct ailo_ggml_opt_params *self,
                                          size_t count, size_t index,
                                          struct ailo_ggml_opt_params **output,
                                          ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_opt_params_backend_sched_get(const struct ailo_ggml_opt_params *self,
                                       struct ailo_ggml_backend_sched **output,
                                       ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_opt_params_backend_sched_set(struct ailo_ggml_opt_params *self,
                                       struct ailo_ggml_backend_sched *value,
                                       ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_opt_params_ctx_compute_get(const struct ailo_ggml_opt_params *self,
                                     struct ailo_ggml_context **output,
                                     ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_opt_params_ctx_compute_set(struct ailo_ggml_opt_params *self,
                                     struct ailo_ggml_context *value,
                                     ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_opt_params_inputs_get(const struct ailo_ggml_opt_params *self,
                                struct ailo_ggml_tensor **output,
                                ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_opt_params_inputs_set(struct ailo_ggml_opt_params *self,
                                            struct ailo_ggml_tensor *value,
                                            ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_opt_params_outputs_get(const struct ailo_ggml_opt_params *self,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_opt_params_outputs_set(struct ailo_ggml_opt_params *self,
                                             struct ailo_ggml_tensor *value,
                                             ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_opt_params_loss_type_get(const struct ailo_ggml_opt_params *self,
                                   enum ailo_ggml_opt_loss_type *output,
                                   ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_opt_params_loss_type_set(struct ailo_ggml_opt_params *self,
                                   enum ailo_ggml_opt_loss_type value,
                                   ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_opt_params_build_type_get(const struct ailo_ggml_opt_params *self,
                                    enum ailo_ggml_opt_build_type *output,
                                    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_opt_params_build_type_set(struct ailo_ggml_opt_params *self,
                                    enum ailo_ggml_opt_build_type value,
                                    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_opt_params_opt_period_get(const struct ailo_ggml_opt_params *self,
                                    ailo_i32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_opt_params_opt_period_set(struct ailo_ggml_opt_params *self,
                                    ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_opt_params_get_opt_pars_ud_get(
    const struct ailo_ggml_opt_params *self, void **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_opt_params_get_opt_pars_ud_set(struct ailo_ggml_opt_params *self,
                                         void *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_opt_params_optimizer_get(const struct ailo_ggml_opt_params *self,
                                   enum ailo_ggml_opt_optimizer_type *output,
                                   ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_opt_params_optimizer_set(struct ailo_ggml_opt_params *self,
                                   enum ailo_ggml_opt_optimizer_type value,
                                   ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_tallocr_array_new(size_t count,
                                        struct ailo_ggml_tallocr **output,
                                        ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_ggml_tallocr_array_free(struct ailo_ggml_tallocr *self,
                                         ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_tallocr_array_at(struct ailo_ggml_tallocr *self,
                                       size_t count, size_t index,
                                       struct ailo_ggml_tallocr **output,
                                       ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_tallocr_buffer_get(const struct ailo_ggml_tallocr *self,
                             struct ailo_ggml_backend_buffer **output,
                             ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tallocr_buffer_set(struct ailo_ggml_tallocr *self,
                                         struct ailo_ggml_backend_buffer *value,
                                         ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tallocr_base_get(const struct ailo_ggml_tallocr *self,
                                       void **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tallocr_base_set(struct ailo_ggml_tallocr *self,
                                       void *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_tallocr_alignment_get(const struct ailo_ggml_tallocr *self,
                                size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tallocr_alignment_set(struct ailo_ggml_tallocr *self,
                                            size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tallocr_offset_get(const struct ailo_ggml_tallocr *self,
                                         size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tallocr_offset_set(struct ailo_ggml_tallocr *self,
                                         size_t value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_tensor_array_new(size_t count,
                                       struct ailo_ggml_tensor **output,
                                       ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_ggml_tensor_array_free(struct ailo_ggml_tensor *self,
                                        ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_tensor_array_at(struct ailo_ggml_tensor *self,
                                      size_t count, size_t index,
                                      struct ailo_ggml_tensor **output,
                                      ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_type_get(const struct ailo_ggml_tensor *self,
                                      enum ailo_ggml_type *output,
                                      ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_type_set(struct ailo_ggml_tensor *self,
                                      enum ailo_ggml_type value,
                                      ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_tensor_buffer_get(const struct ailo_ggml_tensor *self,
                            struct ailo_ggml_backend_buffer **output,
                            ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_buffer_set(struct ailo_ggml_tensor *self,
                                        struct ailo_ggml_backend_buffer *value,
                                        ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_ne_get(const struct ailo_ggml_tensor *self,
                                    size_t index, ailo_i64 *output,
                                    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_ne_set(struct ailo_ggml_tensor *self, size_t index,
                                    ailo_i64 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_nb_get(const struct ailo_ggml_tensor *self,
                                    size_t index, size_t *output,
                                    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_nb_set(struct ailo_ggml_tensor *self, size_t index,
                                    size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_op_get(const struct ailo_ggml_tensor *self,
                                    enum ailo_ggml_op *output,
                                    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_op_set(struct ailo_ggml_tensor *self,
                                    enum ailo_ggml_op value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_op_params_get(const struct ailo_ggml_tensor *self,
                                           size_t index, ailo_i32 *output,
                                           ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_op_params_set(struct ailo_ggml_tensor *self,
                                           size_t index, ailo_i32 value,
                                           ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_flags_get(const struct ailo_ggml_tensor *self,
                                       ailo_i32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_flags_set(struct ailo_ggml_tensor *self,
                                       ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_src_get(const struct ailo_ggml_tensor *self,
                                     size_t index,
                                     struct ailo_ggml_tensor **output,
                                     ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_src_set(struct ailo_ggml_tensor *self,
                                     size_t index,
                                     struct ailo_ggml_tensor *value,
                                     ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_view_src_get(const struct ailo_ggml_tensor *self,
                                          struct ailo_ggml_tensor **output,
                                          ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_view_src_set(struct ailo_ggml_tensor *self,
                                          struct ailo_ggml_tensor *value,
                                          ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_view_offs_get(const struct ailo_ggml_tensor *self,
                                           size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_view_offs_set(struct ailo_ggml_tensor *self,
                                           size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_data_get(const struct ailo_ggml_tensor *self,
                                      void **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_data_set(struct ailo_ggml_tensor *self,
                                      void *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_name_get(const struct ailo_ggml_tensor *self,
                                      size_t index, char *output,
                                      ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_name_set(struct ailo_ggml_tensor *self,
                                      size_t index, char value,
                                      ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_extra_get(const struct ailo_ggml_tensor *self,
                                       void **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_extra_set(struct ailo_ggml_tensor *self,
                                       void *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_tensor_padding_get(const struct ailo_ggml_tensor *self,
                                         size_t index, char *output,
                                         ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_tensor_padding_set(struct ailo_ggml_tensor *self,
                                         size_t index, char value,
                                         ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_threadpool_params_array_new(
    size_t count, struct ailo_ggml_threadpool_params **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_ggml_threadpool_params_array_free(struct ailo_ggml_threadpool_params *self,
                                       ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_threadpool_params_array_at(
    struct ailo_ggml_threadpool_params *self, size_t count, size_t index,
    struct ailo_ggml_threadpool_params **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_threadpool_params_cpumask_get(
    const struct ailo_ggml_threadpool_params *self, size_t index,
    ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_threadpool_params_cpumask_set(
    struct ailo_ggml_threadpool_params *self, size_t index, ailo_bool value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_threadpool_params_n_threads_get(
    const struct ailo_ggml_threadpool_params *self, int *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_threadpool_params_n_threads_set(
    struct ailo_ggml_threadpool_params *self, int value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_threadpool_params_prio_get(
    const struct ailo_ggml_threadpool_params *self,
    enum ailo_ggml_sched_priority *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_threadpool_params_prio_set(struct ailo_ggml_threadpool_params *self,
                                     enum ailo_ggml_sched_priority value,
                                     ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_threadpool_params_poll_get(
    const struct ailo_ggml_threadpool_params *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_threadpool_params_poll_set(struct ailo_ggml_threadpool_params *self,
                                     ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_threadpool_params_strict_cpu_get(
    const struct ailo_ggml_threadpool_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_threadpool_params_strict_cpu_set(
    struct ailo_ggml_threadpool_params *self, ailo_bool value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_threadpool_params_paused_get(
    const struct ailo_ggml_threadpool_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_threadpool_params_paused_set(struct ailo_ggml_threadpool_params *self,
                                       ailo_bool value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_type_traits_array_new(
    size_t count, struct ailo_ggml_type_traits **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_ggml_type_traits_array_free(struct ailo_ggml_type_traits *self,
                                             ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_type_traits_array_at(
    struct ailo_ggml_type_traits *self, size_t count, size_t index,
    struct ailo_ggml_type_traits **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_type_traits_type_name_get(const struct ailo_ggml_type_traits *self,
                                    const char **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_type_traits_type_name_set(struct ailo_ggml_type_traits *self,
                                    const char *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_type_traits_blck_size_get(const struct ailo_ggml_type_traits *self,
                                    ailo_i64 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_type_traits_blck_size_set(struct ailo_ggml_type_traits *self,
                                    ailo_i64 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_type_traits_blck_size_interleave_get(
    const struct ailo_ggml_type_traits *self, ailo_i64 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_type_traits_blck_size_interleave_set(
    struct ailo_ggml_type_traits *self, ailo_i64 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_type_traits_type_size_get(const struct ailo_ggml_type_traits *self,
                                    size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_type_traits_type_size_set(struct ailo_ggml_type_traits *self,
                                    size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_ggml_type_traits_is_quantized_get(const struct ailo_ggml_type_traits *self,
                                       ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_type_traits_is_quantized_set(struct ailo_ggml_type_traits *self,
                                       ailo_bool value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_ggml_type_traits_cpu_array_new(
    size_t count, struct ailo_ggml_type_traits_cpu **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_ggml_type_traits_cpu_array_free(struct ailo_ggml_type_traits_cpu *self,
                                     ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_ggml_type_traits_cpu_array_at(
    struct ailo_ggml_type_traits_cpu *self, size_t count, size_t index,
    struct ailo_ggml_type_traits_cpu **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_type_traits_cpu_vec_dot_type_get(
    const struct ailo_ggml_type_traits_cpu *self, enum ailo_ggml_type *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_ggml_type_traits_cpu_vec_dot_type_set(
    struct ailo_ggml_type_traits_cpu *self, enum ailo_ggml_type value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_ggml_type_traits_cpu_nrows_get(
    const struct ailo_ggml_type_traits_cpu *self, ailo_i64 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_ggml_type_traits_cpu_nrows_set(struct ailo_ggml_type_traits_cpu *self,
                                    ailo_i64 value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_gguf_init_params_array_new(
    size_t count, struct ailo_gguf_init_params **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_gguf_init_params_array_free(struct ailo_gguf_init_params *self,
                                             ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_gguf_init_params_array_at(
    struct ailo_gguf_init_params *self, size_t count, size_t index,
    struct ailo_gguf_init_params **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_gguf_init_params_no_alloc_get(const struct ailo_gguf_init_params *self,
                                   ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_gguf_init_params_no_alloc_set(struct ailo_gguf_init_params *self,
                                   ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_gguf_init_params_ctx_get(const struct ailo_gguf_init_params *self,
                              struct ailo_storage_struct_ggml_context **output,
                              ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_gguf_init_params_ctx_set(struct ailo_gguf_init_params *self,
                              struct ailo_storage_struct_ggml_context *value,
                              ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_batch_array_new(size_t count,
                                       struct ailo_llama_batch **output,
                                       ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_batch_array_free(struct ailo_llama_batch *self,
                                        ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_batch_array_at(struct ailo_llama_batch *self,
                                      size_t count, size_t index,
                                      struct ailo_llama_batch **output,
                                      ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_batch_n_tokens_get(const struct ailo_llama_batch *self,
                                          ailo_i32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_batch_n_tokens_set(struct ailo_llama_batch *self,
                                          ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_batch_token_get(const struct ailo_llama_batch *self,
                                       ailo_i32 **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_batch_token_set(struct ailo_llama_batch *self,
                                       ailo_i32 *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_batch_embd_get(const struct ailo_llama_batch *self,
                                      float **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_batch_embd_set(struct ailo_llama_batch *self,
                                      float *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_batch_pos_get(const struct ailo_llama_batch *self,
                                     ailo_i32 **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_batch_pos_set(struct ailo_llama_batch *self,
                                     ailo_i32 *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_batch_n_seq_id_get(const struct ailo_llama_batch *self,
                                          ailo_i32 **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_batch_n_seq_id_set(struct ailo_llama_batch *self,
                                          ailo_i32 *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_batch_seq_id_get(const struct ailo_llama_batch *self,
                                        ailo_i32 ***output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_batch_seq_id_set(struct ailo_llama_batch *self,
                                        ailo_i32 **value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_batch_logits_get(const struct ailo_llama_batch *self,
                                        ailo_i8 **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_batch_logits_set(struct ailo_llama_batch *self,
                                        ailo_i8 *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_chat_message_array_new(
    size_t count, struct ailo_llama_chat_message **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_llama_chat_message_array_free(struct ailo_llama_chat_message *self,
                                   ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_chat_message_array_at(
    struct ailo_llama_chat_message *self, size_t count, size_t index,
    struct ailo_llama_chat_message **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_chat_message_role_get(const struct ailo_llama_chat_message *self,
                                 const char **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_chat_message_role_set(struct ailo_llama_chat_message *self,
                                 const char *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_chat_message_content_get(const struct ailo_llama_chat_message *self,
                                    const char **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_chat_message_content_set(struct ailo_llama_chat_message *self,
                                    const char *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_context_params_array_new(
    size_t count, struct ailo_llama_context_params **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_llama_context_params_array_free(struct ailo_llama_context_params *self,
                                     ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_context_params_array_at(
    struct ailo_llama_context_params *self, size_t count, size_t index,
    struct ailo_llama_context_params **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_n_ctx_get(
    const struct ailo_llama_context_params *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_n_ctx_set(struct ailo_llama_context_params *self,
                                    ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_n_batch_get(
    const struct ailo_llama_context_params *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_n_batch_set(struct ailo_llama_context_params *self,
                                      ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_n_ubatch_get(
    const struct ailo_llama_context_params *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_n_ubatch_set(struct ailo_llama_context_params *self,
                                       ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_n_seq_max_get(
    const struct ailo_llama_context_params *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_n_seq_max_set(struct ailo_llama_context_params *self,
                                        ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_n_rs_seq_get(
    const struct ailo_llama_context_params *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_n_rs_seq_set(struct ailo_llama_context_params *self,
                                       ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_n_outputs_max_get(
    const struct ailo_llama_context_params *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_n_outputs_max_set(
    struct ailo_llama_context_params *self, ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_n_outputs_max_per_seq_get(
    const struct ailo_llama_context_params *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_n_outputs_max_per_seq_set(
    struct ailo_llama_context_params *self, ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_n_threads_get(
    const struct ailo_llama_context_params *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_n_threads_set(struct ailo_llama_context_params *self,
                                        ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_n_threads_batch_get(
    const struct ailo_llama_context_params *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_n_threads_batch_set(
    struct ailo_llama_context_params *self, ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_ctx_type_get(
    const struct ailo_llama_context_params *self,
    enum ailo_llama_context_type *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_ctx_type_set(struct ailo_llama_context_params *self,
                                       enum ailo_llama_context_type value,
                                       ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_rope_scaling_type_get(
    const struct ailo_llama_context_params *self,
    enum ailo_llama_rope_scaling_type *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_rope_scaling_type_set(
    struct ailo_llama_context_params *self,
    enum ailo_llama_rope_scaling_type value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_pooling_type_get(
    const struct ailo_llama_context_params *self,
    enum ailo_llama_pooling_type *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_pooling_type_set(
    struct ailo_llama_context_params *self, enum ailo_llama_pooling_type value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_attention_type_get(
    const struct ailo_llama_context_params *self,
    enum ailo_llama_attention_type *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_attention_type_set(
    struct ailo_llama_context_params *self,
    enum ailo_llama_attention_type value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_flash_attn_type_get(
    const struct ailo_llama_context_params *self,
    enum ailo_llama_flash_attn_type *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_flash_attn_type_set(
    struct ailo_llama_context_params *self,
    enum ailo_llama_flash_attn_type value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_rope_freq_base_get(
    const struct ailo_llama_context_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_rope_freq_base_set(
    struct ailo_llama_context_params *self, float value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_rope_freq_scale_get(
    const struct ailo_llama_context_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_rope_freq_scale_set(
    struct ailo_llama_context_params *self, float value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_yarn_ext_factor_get(
    const struct ailo_llama_context_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_yarn_ext_factor_set(
    struct ailo_llama_context_params *self, float value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_yarn_attn_factor_get(
    const struct ailo_llama_context_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_yarn_attn_factor_set(
    struct ailo_llama_context_params *self, float value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_yarn_beta_fast_get(
    const struct ailo_llama_context_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_yarn_beta_fast_set(
    struct ailo_llama_context_params *self, float value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_yarn_beta_slow_get(
    const struct ailo_llama_context_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_yarn_beta_slow_set(
    struct ailo_llama_context_params *self, float value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_yarn_orig_ctx_get(
    const struct ailo_llama_context_params *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_yarn_orig_ctx_set(
    struct ailo_llama_context_params *self, ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_defrag_thold_get(
    const struct ailo_llama_context_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_defrag_thold_set(
    struct ailo_llama_context_params *self, float value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_cb_eval_user_data_get(
    const struct ailo_llama_context_params *self, void **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_cb_eval_user_data_set(
    struct ailo_llama_context_params *self, void *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_type_k_get(
    const struct ailo_llama_context_params *self, enum ailo_ggml_type *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_type_k_set(struct ailo_llama_context_params *self,
                                     enum ailo_ggml_type value,
                                     ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_type_v_get(
    const struct ailo_llama_context_params *self, enum ailo_ggml_type *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_type_v_set(struct ailo_llama_context_params *self,
                                     enum ailo_ggml_type value,
                                     ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_abort_callback_data_get(
    const struct ailo_llama_context_params *self, void **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_abort_callback_data_set(
    struct ailo_llama_context_params *self, void *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_embeddings_get(
    const struct ailo_llama_context_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_embeddings_set(struct ailo_llama_context_params *self,
                                         ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_offload_kqv_get(
    const struct ailo_llama_context_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_offload_kqv_set(
    struct ailo_llama_context_params *self, ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_no_perf_get(
    const struct ailo_llama_context_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_no_perf_set(struct ailo_llama_context_params *self,
                                      ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_op_offload_get(
    const struct ailo_llama_context_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_op_offload_set(struct ailo_llama_context_params *self,
                                         ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_swa_full_get(
    const struct ailo_llama_context_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_swa_full_set(struct ailo_llama_context_params *self,
                                       ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_kv_unified_get(
    const struct ailo_llama_context_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_kv_unified_set(struct ailo_llama_context_params *self,
                                         ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_samplers_get(
    const struct ailo_llama_context_params *self,
    struct ailo_llama_sampler_seq_config **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_context_params_samplers_set(
    struct ailo_llama_context_params *self,
    struct ailo_llama_sampler_seq_config *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_n_samplers_get(
    const struct ailo_llama_context_params *self, size_t *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_n_samplers_set(struct ailo_llama_context_params *self,
                                         size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_context_params_ctx_other_get(
    const struct ailo_llama_context_params *self,
    struct ailo_llama_context **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_context_params_ctx_other_set(struct ailo_llama_context_params *self,
                                        struct ailo_llama_context *value,
                                        ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_embd_array_new(size_t count,
                                      struct ailo_llama_embd **output,
                                      ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_embd_array_free(struct ailo_llama_embd *self,
                                       ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_embd_array_at(struct ailo_llama_embd *self, size_t count,
                                     size_t index,
                                     struct ailo_llama_embd **output,
                                     ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_embd_data_get(const struct ailo_llama_embd *self,
                                     const float **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_embd_data_set(struct ailo_llama_embd *self,
                                     const float *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_embd_n_rows_get(const struct ailo_llama_embd *self,
                                       size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_embd_n_rows_set(struct ailo_llama_embd *self,
                                       size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_embd_n_embd_get(const struct ailo_llama_embd *self,
                                       size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_embd_n_embd_set(struct ailo_llama_embd *self,
                                       size_t value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_logit_bias_array_new(
    size_t count, struct ailo_llama_logit_bias **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_logit_bias_array_free(struct ailo_llama_logit_bias *self,
                                             ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_logit_bias_array_at(
    struct ailo_llama_logit_bias *self, size_t count, size_t index,
    struct ailo_llama_logit_bias **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_logit_bias_token_get(const struct ailo_llama_logit_bias *self,
                                ailo_i32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_logit_bias_token_set(struct ailo_llama_logit_bias *self,
                                            ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_logit_bias_bias_get(const struct ailo_llama_logit_bias *self,
                               float *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_logit_bias_bias_set(struct ailo_llama_logit_bias *self,
                                           float value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_model_imatrix_data_array_new(
    size_t count, struct ailo_llama_model_imatrix_data **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_model_imatrix_data_array_free(
    struct ailo_llama_model_imatrix_data *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_model_imatrix_data_array_at(
    struct ailo_llama_model_imatrix_data *self, size_t count, size_t index,
    struct ailo_llama_model_imatrix_data **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_imatrix_data_name_get(
    const struct ailo_llama_model_imatrix_data *self, const char **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_imatrix_data_name_set(
    struct ailo_llama_model_imatrix_data *self, const char *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_imatrix_data_data_get(
    const struct ailo_llama_model_imatrix_data *self, const float **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_imatrix_data_data_set(
    struct ailo_llama_model_imatrix_data *self, const float *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_imatrix_data_size_get(
    const struct ailo_llama_model_imatrix_data *self, size_t *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_imatrix_data_size_set(
    struct ailo_llama_model_imatrix_data *self, size_t value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_model_kv_override_array_new(
    size_t count, struct ailo_llama_model_kv_override **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_model_kv_override_array_free(
    struct ailo_llama_model_kv_override *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_model_kv_override_array_at(
    struct ailo_llama_model_kv_override *self, size_t count, size_t index,
    struct ailo_llama_model_kv_override **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_kv_override_tag_get(
    const struct ailo_llama_model_kv_override *self,
    enum ailo_llama_model_kv_override_type *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_kv_override_tag_set(
    struct ailo_llama_model_kv_override *self,
    enum ailo_llama_model_kv_override_type value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_kv_override_key_get(
    const struct ailo_llama_model_kv_override *self, size_t index, char *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_model_kv_override_key_set(struct ailo_llama_model_kv_override *self,
                                     size_t index, char value,
                                     ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_kv_override_val_i64_get(
    const struct ailo_llama_model_kv_override *self, ailo_i64 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_kv_override_val_i64_set(
    struct ailo_llama_model_kv_override *self, ailo_i64 value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_kv_override_val_f64_get(
    const struct ailo_llama_model_kv_override *self, double *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_kv_override_val_f64_set(
    struct ailo_llama_model_kv_override *self, double value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_kv_override_val_bool_get(
    const struct ailo_llama_model_kv_override *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_kv_override_val_bool_set(
    struct ailo_llama_model_kv_override *self, ailo_bool value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_kv_override_val_str_get(
    const struct ailo_llama_model_kv_override *self, size_t index, char *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_kv_override_val_str_set(
    struct ailo_llama_model_kv_override *self, size_t index, char value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_model_params_array_new(
    size_t count, struct ailo_llama_model_params **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_llama_model_params_array_free(struct ailo_llama_model_params *self,
                                   ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_model_params_array_at(
    struct ailo_llama_model_params *self, size_t count, size_t index,
    struct ailo_llama_model_params **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_params_devices_get(
    const struct ailo_llama_model_params *self,
    struct ailo_storage_struct_ggml_backend_device **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_params_devices_set(
    struct ailo_llama_model_params *self,
    struct ailo_storage_struct_ggml_backend_device *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_params_tensor_buft_overrides_get(
    const struct ailo_llama_model_params *self,
    const struct ailo_llama_model_tensor_buft_override **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_params_tensor_buft_overrides_set(
    struct ailo_llama_model_params *self,
    const struct ailo_llama_model_tensor_buft_override *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_params_n_gpu_layers_get(
    const struct ailo_llama_model_params *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_model_params_n_gpu_layers_set(struct ailo_llama_model_params *self,
                                         ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_params_split_mode_get(
    const struct ailo_llama_model_params *self,
    enum ailo_llama_split_mode *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_model_params_split_mode_set(struct ailo_llama_model_params *self,
                                       enum ailo_llama_split_mode value,
                                       ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_params_load_mode_get(
    const struct ailo_llama_model_params *self,
    enum ailo_llama_load_mode *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_model_params_load_mode_set(struct ailo_llama_model_params *self,
                                      enum ailo_llama_load_mode value,
                                      ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_params_lazy_mode_get(
    const struct ailo_llama_model_params *self,
    enum ailo_llama_lazy_mode *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_model_params_lazy_mode_set(struct ailo_llama_model_params *self,
                                      enum ailo_llama_lazy_mode value,
                                      ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_model_params_main_gpu_get(const struct ailo_llama_model_params *self,
                                     ailo_i32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_model_params_main_gpu_set(struct ailo_llama_model_params *self,
                                     ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_params_tensor_split_get(
    const struct ailo_llama_model_params *self, const float **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_model_params_tensor_split_set(struct ailo_llama_model_params *self,
                                         const float *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_params_progress_callback_user_data_get(
    const struct ailo_llama_model_params *self, void **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_params_progress_callback_user_data_set(
    struct ailo_llama_model_params *self, void *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_params_kv_overrides_get(
    const struct ailo_llama_model_params *self,
    const struct ailo_llama_model_kv_override **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_params_kv_overrides_set(
    struct ailo_llama_model_params *self,
    const struct ailo_llama_model_kv_override *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_params_vocab_only_get(
    const struct ailo_llama_model_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_model_params_vocab_only_set(struct ailo_llama_model_params *self,
                                       ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_params_check_tensors_get(
    const struct ailo_llama_model_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_model_params_check_tensors_set(struct ailo_llama_model_params *self,
                                          ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_params_use_extra_bufts_get(
    const struct ailo_llama_model_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_params_use_extra_bufts_set(
    struct ailo_llama_model_params *self, ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_model_params_no_host_get(const struct ailo_llama_model_params *self,
                                    ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_model_params_no_host_set(struct ailo_llama_model_params *self,
                                    ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_model_params_no_alloc_get(const struct ailo_llama_model_params *self,
                                     ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_model_params_no_alloc_set(struct ailo_llama_model_params *self,
                                     ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_model_params_load_mtp_get(const struct ailo_llama_model_params *self,
                                     ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_model_params_load_mtp_set(struct ailo_llama_model_params *self,
                                     ailo_bool value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_model_quantize_params_array_new(
    size_t count, struct ailo_llama_model_quantize_params **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_model_quantize_params_array_free(
    struct ailo_llama_model_quantize_params *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_model_quantize_params_array_at(
    struct ailo_llama_model_quantize_params *self, size_t count, size_t index,
    struct ailo_llama_model_quantize_params **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_nthread_get(
    const struct ailo_llama_model_quantize_params *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_nthread_set(
    struct ailo_llama_model_quantize_params *self, ailo_i32 value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_ftype_get(
    const struct ailo_llama_model_quantize_params *self,
    enum ailo_llama_ftype *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_ftype_set(
    struct ailo_llama_model_quantize_params *self, enum ailo_llama_ftype value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_output_tensor_type_get(
    const struct ailo_llama_model_quantize_params *self,
    enum ailo_ggml_type *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_output_tensor_type_set(
    struct ailo_llama_model_quantize_params *self, enum ailo_ggml_type value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_token_embedding_type_get(
    const struct ailo_llama_model_quantize_params *self,
    enum ailo_ggml_type *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_token_embedding_type_set(
    struct ailo_llama_model_quantize_params *self, enum ailo_ggml_type value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_allow_requantize_get(
    const struct ailo_llama_model_quantize_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_allow_requantize_set(
    struct ailo_llama_model_quantize_params *self, ailo_bool value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_quantize_output_tensor_get(
    const struct ailo_llama_model_quantize_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_quantize_output_tensor_set(
    struct ailo_llama_model_quantize_params *self, ailo_bool value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_only_copy_get(
    const struct ailo_llama_model_quantize_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_only_copy_set(
    struct ailo_llama_model_quantize_params *self, ailo_bool value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_pure_get(
    const struct ailo_llama_model_quantize_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_pure_set(
    struct ailo_llama_model_quantize_params *self, ailo_bool value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_keep_split_get(
    const struct ailo_llama_model_quantize_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_keep_split_set(
    struct ailo_llama_model_quantize_params *self, ailo_bool value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_dry_run_get(
    const struct ailo_llama_model_quantize_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_dry_run_set(
    struct ailo_llama_model_quantize_params *self, ailo_bool value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_imatrix_get(
    const struct ailo_llama_model_quantize_params *self,
    const struct ailo_llama_model_imatrix_data **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_imatrix_set(
    struct ailo_llama_model_quantize_params *self,
    const struct ailo_llama_model_imatrix_data *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_kv_overrides_get(
    const struct ailo_llama_model_quantize_params *self,
    const struct ailo_llama_model_kv_override **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_kv_overrides_set(
    struct ailo_llama_model_quantize_params *self,
    const struct ailo_llama_model_kv_override *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_tt_overrides_get(
    const struct ailo_llama_model_quantize_params *self,
    const struct ailo_llama_model_tensor_override **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_tt_overrides_set(
    struct ailo_llama_model_quantize_params *self,
    const struct ailo_llama_model_tensor_override *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_prune_layers_get(
    const struct ailo_llama_model_quantize_params *self,
    const ailo_i32 **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_prune_layers_set(
    struct ailo_llama_model_quantize_params *self, const ailo_i32 *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_quantize_params_max_buf_size_get(
    const struct ailo_llama_model_quantize_params *self, size_t *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_quantize_params_max_buf_size_set(
    struct ailo_llama_model_quantize_params *self, size_t value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_model_tensor_buft_override_array_new(
    size_t count, struct ailo_llama_model_tensor_buft_override **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_model_tensor_buft_override_array_free(
    struct ailo_llama_model_tensor_buft_override *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_model_tensor_buft_override_array_at(
    struct ailo_llama_model_tensor_buft_override *self, size_t count,
    size_t index, struct ailo_llama_model_tensor_buft_override **output,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_tensor_buft_override_pattern_get(
    const struct ailo_llama_model_tensor_buft_override *self,
    const char **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_tensor_buft_override_pattern_set(
    struct ailo_llama_model_tensor_buft_override *self, const char *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_tensor_buft_override_buft_get(
    const struct ailo_llama_model_tensor_buft_override *self,
    struct ailo_ggml_backend_buffer_type **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_tensor_buft_override_buft_set(
    struct ailo_llama_model_tensor_buft_override *self,
    struct ailo_ggml_backend_buffer_type *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_model_tensor_override_array_new(
    size_t count, struct ailo_llama_model_tensor_override **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_model_tensor_override_array_free(
    struct ailo_llama_model_tensor_override *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_model_tensor_override_array_at(
    struct ailo_llama_model_tensor_override *self, size_t count, size_t index,
    struct ailo_llama_model_tensor_override **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_tensor_override_pattern_get(
    const struct ailo_llama_model_tensor_override *self, const char **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_tensor_override_pattern_set(
    struct ailo_llama_model_tensor_override *self, const char *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_model_tensor_override_type_get(
    const struct ailo_llama_model_tensor_override *self,
    enum ailo_ggml_type *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_model_tensor_override_type_set(
    struct ailo_llama_model_tensor_override *self, enum ailo_ggml_type value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_opt_params_array_new(
    size_t count, struct ailo_llama_opt_params **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_opt_params_array_free(struct ailo_llama_opt_params *self,
                                             ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_opt_params_array_at(
    struct ailo_llama_opt_params *self, size_t count, size_t index,
    struct ailo_llama_opt_params **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_opt_params_n_ctx_train_get(const struct ailo_llama_opt_params *self,
                                      ailo_u32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_opt_params_n_ctx_train_set(struct ailo_llama_opt_params *self,
                                      ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_opt_params_param_filter_ud_get(
    const struct ailo_llama_opt_params *self, void **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_opt_params_param_filter_ud_set(struct ailo_llama_opt_params *self,
                                          void *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_opt_params_get_opt_pars_ud_get(
    const struct ailo_llama_opt_params *self, void **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_opt_params_get_opt_pars_ud_set(struct ailo_llama_opt_params *self,
                                          void *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_opt_params_optimizer_type_get(
    const struct ailo_llama_opt_params *self,
    enum ailo_ggml_opt_optimizer_type *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_opt_params_optimizer_type_set(
    struct ailo_llama_opt_params *self, enum ailo_ggml_opt_optimizer_type value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_perf_context_data_array_new(
    size_t count, struct ailo_llama_perf_context_data **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_perf_context_data_array_free(
    struct ailo_llama_perf_context_data *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_perf_context_data_array_at(
    struct ailo_llama_perf_context_data *self, size_t count, size_t index,
    struct ailo_llama_perf_context_data **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_perf_context_data_t_start_ms_get(
    const struct ailo_llama_perf_context_data *self, double *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_perf_context_data_t_start_ms_set(
    struct ailo_llama_perf_context_data *self, double value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_perf_context_data_t_load_ms_get(
    const struct ailo_llama_perf_context_data *self, double *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_perf_context_data_t_load_ms_set(
    struct ailo_llama_perf_context_data *self, double value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_perf_context_data_t_p_eval_ms_get(
    const struct ailo_llama_perf_context_data *self, double *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_perf_context_data_t_p_eval_ms_set(
    struct ailo_llama_perf_context_data *self, double value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_perf_context_data_t_eval_ms_get(
    const struct ailo_llama_perf_context_data *self, double *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_perf_context_data_t_eval_ms_set(
    struct ailo_llama_perf_context_data *self, double value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_perf_context_data_n_p_eval_get(
    const struct ailo_llama_perf_context_data *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_perf_context_data_n_p_eval_set(
    struct ailo_llama_perf_context_data *self, ailo_i32 value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_perf_context_data_n_eval_get(
    const struct ailo_llama_perf_context_data *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_perf_context_data_n_eval_set(
    struct ailo_llama_perf_context_data *self, ailo_i32 value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_perf_context_data_n_reused_get(
    const struct ailo_llama_perf_context_data *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_perf_context_data_n_reused_set(
    struct ailo_llama_perf_context_data *self, ailo_i32 value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_perf_sampler_data_array_new(
    size_t count, struct ailo_llama_perf_sampler_data **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_perf_sampler_data_array_free(
    struct ailo_llama_perf_sampler_data *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_perf_sampler_data_array_at(
    struct ailo_llama_perf_sampler_data *self, size_t count, size_t index,
    struct ailo_llama_perf_sampler_data **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_perf_sampler_data_t_sample_ms_get(
    const struct ailo_llama_perf_sampler_data *self, double *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_perf_sampler_data_t_sample_ms_set(
    struct ailo_llama_perf_sampler_data *self, double value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_perf_sampler_data_n_sample_get(
    const struct ailo_llama_perf_sampler_data *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_perf_sampler_data_n_sample_set(
    struct ailo_llama_perf_sampler_data *self, ailo_i32 value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_sampler_array_new(size_t count,
                                         struct ailo_llama_sampler **output,
                                         ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_sampler_array_free(struct ailo_llama_sampler *self,
                                          ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_sampler_array_at(struct ailo_llama_sampler *self,
                                        size_t count, size_t index,
                                        struct ailo_llama_sampler **output,
                                        ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_sampler_iface_get(const struct ailo_llama_sampler *self,
                                         struct ailo_llama_sampler_i **output,
                                         ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_sampler_iface_set(struct ailo_llama_sampler *self,
                                         struct ailo_llama_sampler_i *value,
                                         ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_sampler_ctx_get(const struct ailo_llama_sampler *self,
                                       void **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_sampler_ctx_set(struct ailo_llama_sampler *self,
                                       void *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_sampler_chain_params_array_new(
    size_t count, struct ailo_llama_sampler_chain_params **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_sampler_chain_params_array_free(
    struct ailo_llama_sampler_chain_params *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_sampler_chain_params_array_at(
    struct ailo_llama_sampler_chain_params *self, size_t count, size_t index,
    struct ailo_llama_sampler_chain_params **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_sampler_chain_params_no_perf_get(
    const struct ailo_llama_sampler_chain_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_sampler_chain_params_no_perf_set(
    struct ailo_llama_sampler_chain_params *self, ailo_bool value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_sampler_data_array_new(
    size_t count, struct ailo_llama_sampler_data **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_llama_sampler_data_array_free(struct ailo_llama_sampler_data *self,
                                   ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_sampler_data_array_at(
    struct ailo_llama_sampler_data *self, size_t count, size_t index,
    struct ailo_llama_sampler_data **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_sampler_data_logits_get(const struct ailo_llama_sampler_data *self,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_sampler_data_logits_set(struct ailo_llama_sampler_data *self,
                                   struct ailo_ggml_tensor *value,
                                   ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_sampler_data_probs_get(const struct ailo_llama_sampler_data *self,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_sampler_data_probs_set(struct ailo_llama_sampler_data *self,
                                  struct ailo_ggml_tensor *value,
                                  ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_sampler_data_sampled_get(const struct ailo_llama_sampler_data *self,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_sampler_data_sampled_set(struct ailo_llama_sampler_data *self,
                                    struct ailo_ggml_tensor *value,
                                    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_sampler_data_candidates_get(
    const struct ailo_llama_sampler_data *self,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_sampler_data_candidates_set(struct ailo_llama_sampler_data *self,
                                       struct ailo_ggml_tensor *value,
                                       ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_sampler_i_array_new(size_t count,
                                           struct ailo_llama_sampler_i **output,
                                           ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_sampler_i_array_free(struct ailo_llama_sampler_i *self,
                                            ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_sampler_i_array_at(struct ailo_llama_sampler_i *self,
                                          size_t count, size_t index,
                                          struct ailo_llama_sampler_i **output,
                                          ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_sampler_seq_config_array_new(
    size_t count, struct ailo_llama_sampler_seq_config **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_sampler_seq_config_array_free(
    struct ailo_llama_sampler_seq_config *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_sampler_seq_config_array_at(
    struct ailo_llama_sampler_seq_config *self, size_t count, size_t index,
    struct ailo_llama_sampler_seq_config **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_sampler_seq_config_seq_id_get(
    const struct ailo_llama_sampler_seq_config *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_sampler_seq_config_seq_id_set(
    struct ailo_llama_sampler_seq_config *self, ailo_i32 value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_sampler_seq_config_sampler_get(
    const struct ailo_llama_sampler_seq_config *self,
    struct ailo_llama_sampler **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_sampler_seq_config_sampler_set(
    struct ailo_llama_sampler_seq_config *self,
    struct ailo_llama_sampler *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_token_data_array_new(
    size_t count, struct ailo_llama_token_data **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_llama_token_data_array_free(struct ailo_llama_token_data *self,
                                             ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_token_data_array_at(
    struct ailo_llama_token_data *self, size_t count, size_t index,
    struct ailo_llama_token_data **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_token_data_id_get(const struct ailo_llama_token_data *self,
                             ailo_i32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_token_data_id_set(struct ailo_llama_token_data *self,
                                         ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_token_data_logit_get(const struct ailo_llama_token_data *self,
                                float *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_token_data_logit_set(struct ailo_llama_token_data *self,
                                            float value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_llama_token_data_p_get(const struct ailo_llama_token_data *self,
                            float *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_token_data_p_set(struct ailo_llama_token_data *self,
                                        float value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_llama_token_data_array_array_new(
    size_t count, struct ailo_llama_token_data_array **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_llama_token_data_array_array_free(struct ailo_llama_token_data_array *self,
                                       ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_llama_token_data_array_array_at(
    struct ailo_llama_token_data_array *self, size_t count, size_t index,
    struct ailo_llama_token_data_array **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_token_data_array_data_get(
    const struct ailo_llama_token_data_array *self,
    struct ailo_llama_token_data **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_token_data_array_data_set(struct ailo_llama_token_data_array *self,
                                     struct ailo_llama_token_data *value,
                                     ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_token_data_array_size_get(
    const struct ailo_llama_token_data_array *self, size_t *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_token_data_array_size_set(struct ailo_llama_token_data_array *self,
                                     size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_token_data_array_selected_get(
    const struct ailo_llama_token_data_array *self, ailo_i64 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_llama_token_data_array_selected_set(
    struct ailo_llama_token_data_array *self, ailo_i64 value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_llama_token_data_array_sorted_get(
    const struct ailo_llama_token_data_array *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_llama_token_data_array_sorted_set(struct ailo_llama_token_data_array *self,
                                       ailo_bool value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_caps_array_new(size_t count,
                                     struct ailo_mtmd_caps **output,
                                     ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_mtmd_caps_array_free(struct ailo_mtmd_caps *self,
                                      ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_caps_array_at(struct ailo_mtmd_caps *self, size_t count,
                                    size_t index,
                                    struct ailo_mtmd_caps **output,
                                    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_caps_inp_vision_get(const struct ailo_mtmd_caps *self,
                                          ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_caps_inp_vision_set(struct ailo_mtmd_caps *self,
                                          ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_caps_inp_audio_get(const struct ailo_mtmd_caps *self,
                                         ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_caps_inp_audio_set(struct ailo_mtmd_caps *self,
                                         ailo_bool value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_context_params_array_new(
    size_t count, struct ailo_mtmd_context_params **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_mtmd_context_params_array_free(struct ailo_mtmd_context_params *self,
                                    ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_context_params_array_at(
    struct ailo_mtmd_context_params *self, size_t count, size_t index,
    struct ailo_mtmd_context_params **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_context_params_use_gpu_get(
    const struct ailo_mtmd_context_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_context_params_use_gpu_set(struct ailo_mtmd_context_params *self,
                                     ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_context_params_device_get(const struct ailo_mtmd_context_params *self,
                                    struct ailo_ggml_backend_device **output,
                                    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_context_params_device_set(struct ailo_mtmd_context_params *self,
                                    struct ailo_ggml_backend_device *value,
                                    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_context_params_print_timings_get(
    const struct ailo_mtmd_context_params *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_context_params_print_timings_set(
    struct ailo_mtmd_context_params *self, ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_context_params_n_threads_get(
    const struct ailo_mtmd_context_params *self, int *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_context_params_n_threads_set(struct ailo_mtmd_context_params *self,
                                       int value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_context_params_image_marker_get(
    const struct ailo_mtmd_context_params *self, const char **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_context_params_image_marker_set(struct ailo_mtmd_context_params *self,
                                          const char *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_context_params_media_marker_get(
    const struct ailo_mtmd_context_params *self, const char **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_context_params_media_marker_set(struct ailo_mtmd_context_params *self,
                                          const char *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_context_params_flash_attn_type_get(
    const struct ailo_mtmd_context_params *self,
    enum ailo_llama_flash_attn_type *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_context_params_flash_attn_type_set(
    struct ailo_mtmd_context_params *self,
    enum ailo_llama_flash_attn_type value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_context_params_warmup_get(const struct ailo_mtmd_context_params *self,
                                    ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_context_params_warmup_set(struct ailo_mtmd_context_params *self,
                                    ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_context_params_image_min_tokens_get(
    const struct ailo_mtmd_context_params *self, int *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_context_params_image_min_tokens_set(
    struct ailo_mtmd_context_params *self, int value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_context_params_image_max_tokens_get(
    const struct ailo_mtmd_context_params *self, int *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_context_params_image_max_tokens_set(
    struct ailo_mtmd_context_params *self, int value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_context_params_cb_eval_user_data_get(
    const struct ailo_mtmd_context_params *self, void **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_context_params_cb_eval_user_data_set(
    struct ailo_mtmd_context_params *self, void *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_context_params_batch_max_tokens_get(
    const struct ailo_mtmd_context_params *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_context_params_batch_max_tokens_set(
    struct ailo_mtmd_context_params *self, ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_context_params_progress_callback_user_data_get(
    const struct ailo_mtmd_context_params *self, void **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_context_params_progress_callback_user_data_set(
    struct ailo_mtmd_context_params *self, void *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_decoder_pos_array_new(
    size_t count, struct ailo_mtmd_decoder_pos **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_mtmd_decoder_pos_array_free(struct ailo_mtmd_decoder_pos *self,
                                             ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_decoder_pos_array_at(
    struct ailo_mtmd_decoder_pos *self, size_t count, size_t index,
    struct ailo_mtmd_decoder_pos **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_decoder_pos_t_get(const struct ailo_mtmd_decoder_pos *self,
                            ailo_u32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_decoder_pos_t_set(struct ailo_mtmd_decoder_pos *self,
                                        ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_decoder_pos_x_get(const struct ailo_mtmd_decoder_pos *self,
                            ailo_u32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_decoder_pos_x_set(struct ailo_mtmd_decoder_pos *self,
                                        ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_decoder_pos_y_get(const struct ailo_mtmd_decoder_pos *self,
                            ailo_u32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_decoder_pos_y_set(struct ailo_mtmd_decoder_pos *self,
                                        ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_decoder_pos_z_get(const struct ailo_mtmd_decoder_pos *self,
                            ailo_u32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_decoder_pos_z_set(struct ailo_mtmd_decoder_pos *self,
                                        ailo_u32 value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_gen_audio_info_array_new(
    size_t count, struct ailo_mtmd_gen_audio_info **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_mtmd_gen_audio_info_array_free(struct ailo_mtmd_gen_audio_info *self,
                                    ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_gen_audio_info_array_at(
    struct ailo_mtmd_gen_audio_info *self, size_t count, size_t index,
    struct ailo_mtmd_gen_audio_info **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_gen_audio_info_type_get(const struct ailo_mtmd_gen_audio_info *self,
                                  enum ailo_mtmd_gen_audio_type *output,
                                  ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_gen_audio_info_type_set(struct ailo_mtmd_gen_audio_info *self,
                                  enum ailo_mtmd_gen_audio_type value,
                                  ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_audio_info_sample_rate_get(
    const struct ailo_mtmd_gen_audio_info *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_gen_audio_info_sample_rate_set(struct ailo_mtmd_gen_audio_info *self,
                                         ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_audio_info_model_variant_get(
    const struct ailo_mtmd_gen_audio_info *self, const char **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_audio_info_model_variant_set(
    struct ailo_mtmd_gen_audio_info *self, const char *value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_gen_inp_array_new(size_t count,
                                        struct ailo_mtmd_gen_inp **output,
                                        ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_mtmd_gen_inp_array_free(struct ailo_mtmd_gen_inp *self,
                                         ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_gen_inp_array_at(struct ailo_mtmd_gen_inp *self,
                                       size_t count, size_t index,
                                       struct ailo_mtmd_gen_inp **output,
                                       ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_inp_type_get(const struct ailo_mtmd_gen_inp *self,
                                       enum ailo_mtmd_gen_process_type *output,
                                       ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_type_set(struct ailo_mtmd_gen_inp *self,
                                       enum ailo_mtmd_gen_process_type value,
                                       ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_inp_code0_get(const struct ailo_mtmd_gen_inp *self,
                                        ailo_i32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_code0_set(struct ailo_mtmd_gen_inp *self,
                                        ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_inp_embd_get(const struct ailo_mtmd_gen_inp *self,
                                       float **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_embd_set(struct ailo_mtmd_gen_inp *self,
                                       float *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_inp_top_k_get(const struct ailo_mtmd_gen_inp *self,
                                        ailo_i32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_top_k_set(struct ailo_mtmd_gen_inp *self,
                                        ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_inp_top_p_get(const struct ailo_mtmd_gen_inp *self,
                                        float *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_top_p_set(struct ailo_mtmd_gen_inp *self,
                                        float value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_inp_seed_get(const struct ailo_mtmd_gen_inp *self,
                                       ailo_u32 *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_seed_set(struct ailo_mtmd_gen_inp *self,
                                       ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_inp_temp_get(const struct ailo_mtmd_gen_inp *self,
                                       float *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_temp_set(struct ailo_mtmd_gen_inp *self,
                                       float value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_inp_codes_get(const struct ailo_mtmd_gen_inp *self,
                                        ailo_i32 **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_codes_set(struct ailo_mtmd_gen_inp *self,
                                        ailo_i32 *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_inp_n_codes_get(const struct ailo_mtmd_gen_inp *self,
                                          size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_n_codes_set(struct ailo_mtmd_gen_inp *self,
                                          size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_inp_feats_get(const struct ailo_mtmd_gen_inp *self,
                                        const float **output,
                                        ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_feats_set(struct ailo_mtmd_gen_inp *self,
                                        const float *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_inp_n_feats_get(const struct ailo_mtmd_gen_inp *self,
                                          size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_n_feats_set(struct ailo_mtmd_gen_inp *self,
                                          size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_gen_inp_state_data_get(const struct ailo_mtmd_gen_inp *self,
                                 const char **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_state_data_set(struct ailo_mtmd_gen_inp *self,
                                             const char *value,
                                             ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_gen_inp_state_size_get(const struct ailo_mtmd_gen_inp *self,
                                 size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_inp_state_size_set(struct ailo_mtmd_gen_inp *self,
                                             size_t value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_gen_out_array_new(size_t count,
                                        struct ailo_mtmd_gen_out **output,
                                        ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_mtmd_gen_out_array_free(struct ailo_mtmd_gen_out *self,
                                         ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_gen_out_array_at(struct ailo_mtmd_gen_out *self,
                                       size_t count, size_t index,
                                       struct ailo_mtmd_gen_out **output,
                                       ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_out_codes_get(const struct ailo_mtmd_gen_out *self,
                                        const ailo_i32 **output,
                                        ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_out_codes_set(struct ailo_mtmd_gen_out *self,
                                        const ailo_i32 *value,
                                        ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_out_n_codes_get(const struct ailo_mtmd_gen_out *self,
                                          size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_out_n_codes_set(struct ailo_mtmd_gen_out *self,
                                          size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_out_feats_get(const struct ailo_mtmd_gen_out *self,
                                        const float **output,
                                        ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_out_feats_set(struct ailo_mtmd_gen_out *self,
                                        const float *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_out_n_feats_get(const struct ailo_mtmd_gen_out *self,
                                          size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_out_n_feats_set(struct ailo_mtmd_gen_out *self,
                                          size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_out_embd_get(const struct ailo_mtmd_gen_out *self,
                                       const float **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_out_embd_set(struct ailo_mtmd_gen_out *self,
                                       const float *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_out_is_eos_get(const struct ailo_mtmd_gen_out *self,
                                         ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_out_is_eos_set(struct ailo_mtmd_gen_out *self,
                                         ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_gen_out_audio_get(const struct ailo_mtmd_gen_out *self,
                                        const float **output,
                                        ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_out_audio_set(struct ailo_mtmd_gen_out *self,
                                        const float *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_gen_out_n_samples_get(const struct ailo_mtmd_gen_out *self,
                                size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_out_n_samples_set(struct ailo_mtmd_gen_out *self,
                                            size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_gen_out_state_data_get(const struct ailo_mtmd_gen_out *self,
                                 const char **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_out_state_data_set(struct ailo_mtmd_gen_out *self,
                                             const char *value,
                                             ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_gen_out_state_size_get(const struct ailo_mtmd_gen_out *self,
                                 size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_gen_out_state_size_set(struct ailo_mtmd_gen_out *self,
                                             size_t value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_helper_bitmap_wrapper_array_new(
    size_t count, struct ailo_mtmd_helper_bitmap_wrapper **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_mtmd_helper_bitmap_wrapper_array_free(
    struct ailo_mtmd_helper_bitmap_wrapper *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_helper_bitmap_wrapper_array_at(
    struct ailo_mtmd_helper_bitmap_wrapper *self, size_t count, size_t index,
    struct ailo_mtmd_helper_bitmap_wrapper **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_bitmap_wrapper_bitmap_get(
    const struct ailo_mtmd_helper_bitmap_wrapper *self,
    struct ailo_mtmd_bitmap **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_bitmap_wrapper_bitmap_set(
    struct ailo_mtmd_helper_bitmap_wrapper *self,
    struct ailo_mtmd_bitmap *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_bitmap_wrapper_video_ctx_get(
    const struct ailo_mtmd_helper_bitmap_wrapper *self,
    struct ailo_mtmd_helper_video **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_bitmap_wrapper_video_ctx_set(
    struct ailo_mtmd_helper_bitmap_wrapper *self,
    struct ailo_mtmd_helper_video *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_helper_embd_batch_array_new(
    size_t count, struct ailo_mtmd_helper_embd_batch **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_mtmd_helper_embd_batch_array_free(struct ailo_mtmd_helper_embd_batch *self,
                                       ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_helper_embd_batch_array_at(
    struct ailo_mtmd_helper_embd_batch *self, size_t count, size_t index,
    struct ailo_mtmd_helper_embd_batch **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_embd_batch_n_tokens_get(
    const struct ailo_mtmd_helper_embd_batch *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_embd_batch_n_tokens_set(
    struct ailo_mtmd_helper_embd_batch *self, ailo_i32 value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_embd_batch_embd_get(
    const struct ailo_mtmd_helper_embd_batch *self, const float **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_helper_embd_batch_embd_set(struct ailo_mtmd_helper_embd_batch *self,
                                     const float *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_embd_batch_n_embd_get(
    const struct ailo_mtmd_helper_embd_batch *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_helper_embd_batch_n_embd_set(struct ailo_mtmd_helper_embd_batch *self,
                                       ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_embd_batch_pos_get(
    const struct ailo_mtmd_helper_embd_batch *self, const ailo_i32 **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_helper_embd_batch_pos_set(struct ailo_mtmd_helper_embd_batch *self,
                                    const ailo_i32 *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_embd_batch_n_pos_get(
    const struct ailo_mtmd_helper_embd_batch *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_helper_embd_batch_n_pos_set(struct ailo_mtmd_helper_embd_batch *self,
                                      ailo_i32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_embd_batch_seq_id_get(
    const struct ailo_mtmd_helper_embd_batch *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_helper_embd_batch_seq_id_set(struct ailo_mtmd_helper_embd_batch *self,
                                       ailo_i32 value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_helper_gen_audio_inp_array_new(
    size_t count, struct ailo_mtmd_helper_gen_audio_inp **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_mtmd_helper_gen_audio_inp_array_free(
    struct ailo_mtmd_helper_gen_audio_inp *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_helper_gen_audio_inp_array_at(
    struct ailo_mtmd_helper_gen_audio_inp *self, size_t count, size_t index,
    struct ailo_mtmd_helper_gen_audio_inp **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_gen_audio_inp_seq_id_get(
    const struct ailo_mtmd_helper_gen_audio_inp *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_gen_audio_inp_seq_id_set(
    struct ailo_mtmd_helper_gen_audio_inp *self, ailo_i32 value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_gen_audio_inp_prompt_get(
    const struct ailo_mtmd_helper_gen_audio_inp *self, const char **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_gen_audio_inp_prompt_set(
    struct ailo_mtmd_helper_gen_audio_inp *self, const char *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_gen_audio_inp_prompt_len_get(
    const struct ailo_mtmd_helper_gen_audio_inp *self, size_t *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_gen_audio_inp_prompt_len_set(
    struct ailo_mtmd_helper_gen_audio_inp *self, size_t value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_gen_audio_inp_speaker_ref_get(
    const struct ailo_mtmd_helper_gen_audio_inp *self,
    struct ailo_mtmd_bitmap **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_gen_audio_inp_speaker_ref_set(
    struct ailo_mtmd_helper_gen_audio_inp *self, struct ailo_mtmd_bitmap *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_gen_audio_inp_lang_get(
    const struct ailo_mtmd_helper_gen_audio_inp *self, const char **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_gen_audio_inp_lang_set(
    struct ailo_mtmd_helper_gen_audio_inp *self, const char *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_gen_audio_inp_top_k_get(
    const struct ailo_mtmd_helper_gen_audio_inp *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_gen_audio_inp_top_k_set(
    struct ailo_mtmd_helper_gen_audio_inp *self, ailo_i32 value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_gen_audio_inp_top_p_get(
    const struct ailo_mtmd_helper_gen_audio_inp *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_gen_audio_inp_top_p_set(
    struct ailo_mtmd_helper_gen_audio_inp *self, float value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_gen_audio_inp_seed_get(
    const struct ailo_mtmd_helper_gen_audio_inp *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_gen_audio_inp_seed_set(
    struct ailo_mtmd_helper_gen_audio_inp *self, ailo_u32 value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_gen_audio_inp_out_type_get(
    const struct ailo_mtmd_helper_gen_audio_inp *self,
    enum ailo_mtmd_helper_gen_audio_outtype *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_gen_audio_inp_out_type_set(
    struct ailo_mtmd_helper_gen_audio_inp *self,
    enum ailo_mtmd_helper_gen_audio_outtype value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_helper_init_opt_array_new(
    size_t count, struct ailo_mtmd_helper_init_opt **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_mtmd_helper_init_opt_array_free(struct ailo_mtmd_helper_init_opt *self,
                                     ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_helper_init_opt_array_at(
    struct ailo_mtmd_helper_init_opt *self, size_t count, size_t index,
    struct ailo_mtmd_helper_init_opt **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_init_opt_video_params_get(
    const struct ailo_mtmd_helper_init_opt *self,
    struct ailo_mtmd_helper_video_init_params **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_init_opt_video_params_set(
    struct ailo_mtmd_helper_init_opt *self,
    const struct ailo_mtmd_helper_video_init_params *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_helper_video_info_array_new(
    size_t count, struct ailo_mtmd_helper_video_info **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_mtmd_helper_video_info_array_free(struct ailo_mtmd_helper_video_info *self,
                                       ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_helper_video_info_array_at(
    struct ailo_mtmd_helper_video_info *self, size_t count, size_t index,
    struct ailo_mtmd_helper_video_info **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_video_info_width_get(
    const struct ailo_mtmd_helper_video_info *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_helper_video_info_width_set(struct ailo_mtmd_helper_video_info *self,
                                      ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_video_info_height_get(
    const struct ailo_mtmd_helper_video_info *self, ailo_u32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_helper_video_info_height_set(struct ailo_mtmd_helper_video_info *self,
                                       ailo_u32 value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_video_info_fps_get(
    const struct ailo_mtmd_helper_video_info *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_helper_video_info_fps_set(struct ailo_mtmd_helper_video_info *self,
                                    float value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_video_info_n_frames_get(
    const struct ailo_mtmd_helper_video_info *self, ailo_i32 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_video_info_n_frames_set(
    struct ailo_mtmd_helper_video_info *self, ailo_i32 value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_helper_video_init_params_array_new(
    size_t count, struct ailo_mtmd_helper_video_init_params **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_mtmd_helper_video_init_params_array_free(
    struct ailo_mtmd_helper_video_init_params *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_helper_video_init_params_array_at(
    struct ailo_mtmd_helper_video_init_params *self, size_t count, size_t index,
    struct ailo_mtmd_helper_video_init_params **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_video_init_params_fps_target_get(
    const struct ailo_mtmd_helper_video_init_params *self, float *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_video_init_params_fps_target_set(
    struct ailo_mtmd_helper_video_init_params *self, float value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_video_init_params_ffmpeg_bin_dir_get(
    const struct ailo_mtmd_helper_video_init_params *self, const char **output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_video_init_params_ffmpeg_bin_dir_set(
    struct ailo_mtmd_helper_video_init_params *self, const char *value,
    ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_helper_video_init_params_timestamp_interval_ms_get(
    const struct ailo_mtmd_helper_video_init_params *self, ailo_i64 *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_helper_video_init_params_timestamp_interval_ms_set(
    struct ailo_mtmd_helper_video_init_params *self, ailo_i64 value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_input_part_array_new(size_t count,
                                           struct ailo_mtmd_input_part **output,
                                           ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_mtmd_input_part_array_free(struct ailo_mtmd_input_part *self,
                                            ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_input_part_array_at(struct ailo_mtmd_input_part *self,
                                          size_t count, size_t index,
                                          struct ailo_mtmd_input_part **output,
                                          ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_input_part_text_get(const struct ailo_mtmd_input_part *self,
                              const struct ailo_mtmd_input_text **output,
                              ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_input_part_text_set(struct ailo_mtmd_input_part *self,
                              const struct ailo_mtmd_input_text *value,
                              ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_input_part_bitmap_get(const struct ailo_mtmd_input_part *self,
                                const struct ailo_mtmd_bitmap **output,
                                ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_input_part_bitmap_set(struct ailo_mtmd_input_part *self,
                                const struct ailo_mtmd_bitmap *value,
                                ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_input_text_array_new(size_t count,
                                           struct ailo_mtmd_input_text **output,
                                           ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_mtmd_input_text_array_free(struct ailo_mtmd_input_text *self,
                                            ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_input_text_array_at(struct ailo_mtmd_input_text *self,
                                          size_t count, size_t index,
                                          struct ailo_mtmd_input_text **output,
                                          ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_input_text_text_get(const struct ailo_mtmd_input_text *self,
                              const char **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_input_text_text_set(struct ailo_mtmd_input_text *self,
                                          const char *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_input_text_text_len_get(const struct ailo_mtmd_input_text *self,
                                  size_t *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_input_text_text_len_set(struct ailo_mtmd_input_text *self,
                                              size_t value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_input_text_add_special_get(const struct ailo_mtmd_input_text *self,
                                     ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_input_text_add_special_set(struct ailo_mtmd_input_text *self,
                                     ailo_bool value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status
ailo_mtmd_input_text_parse_special_get(const struct ailo_mtmd_input_text *self,
                                       ailo_bool *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_input_text_parse_special_set(struct ailo_mtmd_input_text *self,
                                       ailo_bool value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_mtmd_memory_usage_array_new(
    size_t count, struct ailo_mtmd_memory_usage **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_mtmd_memory_usage_array_free(struct ailo_mtmd_memory_usage *self,
                                  ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_mtmd_memory_usage_array_at(
    struct ailo_mtmd_memory_usage *self, size_t count, size_t index,
    struct ailo_mtmd_memory_usage **output, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_memory_usage_backend_mem_usage_get(
    const struct ailo_mtmd_memory_usage *self,
    struct ailo_backend_memory_map **output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status ailo_mtmd_memory_usage_backend_mem_usage_set(
    struct ailo_mtmd_memory_usage *self,
    const struct ailo_backend_memory_map *value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_memory_usage_image_max_tokens_get(
    const struct ailo_mtmd_memory_usage *self, int *output, ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_memory_usage_image_max_tokens_set(struct ailo_mtmd_memory_usage *self,
                                            int value, ailo_error *error);
/** Read the native field with explicit conversion; record-value copies are
 * owned, pointer views borrowed. */
ailo_status ailo_mtmd_memory_usage_use_non_causal_get(
    const struct ailo_mtmd_memory_usage *self, ailo_bool *output,
    ailo_error *error);
/** Mutate the native field; referenced pointer owners must remain alive. */
ailo_status
ailo_mtmd_memory_usage_use_non_causal_set(struct ailo_mtmd_memory_usage *self,
                                          ailo_bool value, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_abs(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_abs_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_acc(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor *argument_2,
                          size_t argument_3, size_t argument_4,
                          size_t argument_5, size_t argument_6,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_acc_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor *argument_2,
                                  size_t argument_3, size_t argument_4,
                                  size_t argument_5, size_t argument_6,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_add(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor *argument_2,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_add1(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           struct ailo_ggml_tensor *argument_2,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_add1_inplace(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor *argument_2,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_add_cast(struct ailo_ggml_context *argument_0,
                               struct ailo_ggml_tensor *argument_1,
                               struct ailo_ggml_tensor *argument_2,
                               enum ailo_ggml_type argument_3,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_add_id(struct ailo_ggml_context *argument_0,
                             struct ailo_ggml_tensor *argument_1,
                             struct ailo_ggml_tensor *argument_2,
                             struct ailo_ggml_tensor *argument_3,
                             struct ailo_ggml_tensor **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_add_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor *argument_2,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_add_rel_pos(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor *argument_2,
                                  struct ailo_ggml_tensor *argument_3,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_add_rel_pos_inplace(struct ailo_ggml_context *argument_0,
                                          struct ailo_ggml_tensor *argument_1,
                                          struct ailo_ggml_tensor *argument_2,
                                          struct ailo_ggml_tensor *argument_3,
                                          struct ailo_ggml_tensor **output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_arange(struct ailo_ggml_context *argument_0,
                             float argument_1, float argument_2,
                             float argument_3, struct ailo_ggml_tensor **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_are_same_shape(const struct ailo_ggml_tensor *argument_0,
                                     const struct ailo_ggml_tensor *argument_1,
                                     ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_are_same_stride(const struct ailo_ggml_tensor *argument_0,
                                      const struct ailo_ggml_tensor *argument_1,
                                      ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_argmax(struct ailo_ggml_context *argument_0,
                             struct ailo_ggml_tensor *argument_1,
                             struct ailo_ggml_tensor **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_argsort(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              enum ailo_ggml_sort_order argument_2,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_argsort_top_k(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    int argument_2,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_alloc_buffer(
    struct ailo_ggml_backend *argument_0, size_t argument_1,
    struct ailo_ggml_backend_buffer **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_alloc_ctx_tensors(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_backend *argument_1,
    struct ailo_ggml_backend_buffer **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_alloc_ctx_tensors_from_buft(
    struct ailo_ggml_context *argument_0,
    struct ailo_ggml_backend_buffer_type *argument_1,
    struct ailo_ggml_backend_buffer **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_alloc_ctx_tensors_from_buft_size(
    struct ailo_ggml_context *argument_0,
    struct ailo_ggml_backend_buffer_type *argument_1, size_t *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_blas_init(struct ailo_ggml_backend **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_blas_reg(struct ailo_ggml_backend_reg **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_blas_set_n_threads(struct ailo_ggml_backend *argument_0,
                                     int argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_buffer_clear(struct ailo_ggml_backend_buffer *argument_0,
                               ailo_u8 argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_buffer_free(struct ailo_ggml_backend_buffer *argument_0,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buffer_get_alignment(
    struct ailo_ggml_backend_buffer *argument_0, size_t *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buffer_get_alloc_size(
    struct ailo_ggml_backend_buffer *argument_0,
    const struct ailo_ggml_tensor *argument_1, size_t *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_buffer_get_base(struct ailo_ggml_backend_buffer *argument_0,
                                  void **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buffer_get_max_size(
    struct ailo_ggml_backend_buffer *argument_0, size_t *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_buffer_get_size(struct ailo_ggml_backend_buffer *argument_0,
                                  size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_buffer_get_type(struct ailo_ggml_backend_buffer *argument_0,
                                  struct ailo_ggml_backend_buffer_type **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_buffer_get_usage(struct ailo_ggml_backend_buffer *argument_0,
                                   enum ailo_ggml_backend_buffer_usage *output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buffer_init_tensor(
    struct ailo_ggml_backend_buffer *argument_0,
    struct ailo_ggml_tensor *argument_1, enum ailo_ggml_status *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_buffer_is_host(struct ailo_ggml_backend_buffer *argument_0,
                                 ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buffer_is_openvino(
    struct ailo_ggml_backend_buffer *argument_0, ailo_bool *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_buffer_name(struct ailo_ggml_backend_buffer *argument_0,
                              const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_buffer_reset(struct ailo_ggml_backend_buffer *argument_0,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buffer_set_usage(
    struct ailo_ggml_backend_buffer *argument_0,
    enum ailo_ggml_backend_buffer_usage argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buft_alloc_buffer(
    struct ailo_ggml_backend_buffer_type *argument_0, size_t argument_1,
    struct ailo_ggml_backend_buffer **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buft_alloc_buffer_n(
    struct ailo_ggml_backend_buffer_type *argument_0,
    struct ailo_storage_struct_ggml_tensor *argument_1, int argument_2,
    struct ailo_ggml_backend_buffer **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buft_get_alignment(
    struct ailo_ggml_backend_buffer_type *argument_0, size_t *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buft_get_alloc_size(
    struct ailo_ggml_backend_buffer_type *argument_0,
    const struct ailo_ggml_tensor *argument_1, size_t *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buft_get_alloc_size_n(
    struct ailo_ggml_backend_buffer_type *argument_0,
    struct ailo_storage_struct_ggml_tensor *argument_1, int argument_2,
    size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buft_get_device(
    struct ailo_ggml_backend_buffer_type *argument_0,
    struct ailo_ggml_backend_device **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buft_get_max_size(
    struct ailo_ggml_backend_buffer_type *argument_0, size_t *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_buft_is_host(struct ailo_ggml_backend_buffer_type *argument_0,
                               ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buft_is_openvino(
    struct ailo_ggml_backend_buffer_type *argument_0, ailo_bool *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_buft_is_openvino_host(
    struct ailo_ggml_backend_buffer_type *argument_0, ailo_bool *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_buft_name(struct ailo_ggml_backend_buffer_type *argument_0,
                            const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cann_buffer_type(
    ailo_i32 argument_0, struct ailo_ggml_backend_buffer_type **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cann_get_device_count(ailo_i32 *output,
                                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cann_get_device_description(ailo_i32 argument_0,
                                                          char *argument_1,
                                                          size_t argument_2,
                                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cann_get_device_memory(ailo_i32 argument_0,
                                                     size_t *argument_1,
                                                     size_t *argument_2,
                                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cann_host_buffer_type(
    struct ailo_ggml_backend_buffer_type **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cann_init(ailo_i32 argument_0,
                                        struct ailo_ggml_backend **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cann_reg(struct ailo_ggml_backend_reg **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_cpu_buffer_from_ptr(void *argument_0, size_t argument_1,
                                      struct ailo_ggml_backend_buffer **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_cpu_buffer_type(struct ailo_ggml_backend_buffer_type **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cpu_init(struct ailo_ggml_backend **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cpu_reg(struct ailo_ggml_backend_reg **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_cpu_set_n_threads(struct ailo_ggml_backend *argument_0,
                                    int argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_cpu_set_threadpool(struct ailo_ggml_backend *argument_0,
                                     struct ailo_ggml_threadpool *argument_1,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_cpu_set_use_ref(struct ailo_ggml_backend *argument_0,
                                  ailo_bool argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cuda_allreduce_tensor(
    struct ailo_storage_struct_ggml_backend *argument_0,
    struct ailo_storage_struct_ggml_tensor *argument_1, size_t argument_2,
    ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cuda_buffer_type(
    int argument_0, struct ailo_ggml_backend_buffer_type **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cuda_get_device_count(int *output,
                                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cuda_get_device_description(int argument_0,
                                                          char *argument_1,
                                                          size_t argument_2,
                                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cuda_get_device_memory(int argument_0,
                                                     size_t *argument_1,
                                                     size_t *argument_2,
                                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cuda_host_buffer_type(
    struct ailo_ggml_backend_buffer_type **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cuda_init(int argument_0,
                                        struct ailo_ggml_backend **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cuda_reg(struct ailo_ggml_backend_reg **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cuda_register_host_buffer(void *argument_0,
                                                        size_t argument_1,
                                                        ailo_bool *output,
                                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_cuda_unregister_host_buffer(void *argument_0,
                                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_dev_backend_reg(struct ailo_ggml_backend_device *argument_0,
                                  struct ailo_ggml_backend_reg **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_dev_buffer_from_host_ptr(
    struct ailo_ggml_backend_device *argument_0, void *argument_1,
    size_t argument_2, size_t argument_3,
    struct ailo_ggml_backend_buffer **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_dev_buffer_type(struct ailo_ggml_backend_device *argument_0,
                                  struct ailo_ggml_backend_buffer_type **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_dev_by_name(const char *argument_0,
                              struct ailo_ggml_backend_device **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_dev_by_type(enum ailo_ggml_backend_dev_type argument_0,
                              struct ailo_ggml_backend_device **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_dev_count(size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_dev_description(struct ailo_ggml_backend_device *argument_0,
                                  const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_dev_get(size_t argument_0,
                                      struct ailo_ggml_backend_device **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_dev_get_props(struct ailo_ggml_backend_device *argument_0,
                                struct ailo_ggml_backend_dev_props *argument_1,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_dev_host_buffer_type(
    struct ailo_ggml_backend_device *argument_0,
    struct ailo_ggml_backend_buffer_type **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_dev_init(
    struct ailo_ggml_backend_device *argument_0, const char *argument_1,
    struct ailo_ggml_backend **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_dev_memory(struct ailo_ggml_backend_device *argument_0,
                             size_t *argument_1, size_t *argument_2,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_dev_name(struct ailo_ggml_backend_device *argument_0,
                           const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_dev_offload_op(struct ailo_ggml_backend_device *argument_0,
                                 const struct ailo_ggml_tensor *argument_1,
                                 ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_dev_supports_buft(
    struct ailo_ggml_backend_device *argument_0,
    struct ailo_ggml_backend_buffer_type *argument_1, ailo_bool *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_dev_supports_op(struct ailo_ggml_backend_device *argument_0,
                                  const struct ailo_ggml_tensor *argument_1,
                                  ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_dev_type(struct ailo_ggml_backend_device *argument_0,
                           enum ailo_ggml_backend_dev_type *output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_device_register(struct ailo_ggml_backend_device *argument_0,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_event_free(struct ailo_ggml_backend_event *argument_0,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_event_new(struct ailo_ggml_backend_device *argument_0,
                            struct ailo_ggml_backend_event **output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_event_record(struct ailo_ggml_backend_event *argument_0,
                               struct ailo_ggml_backend *argument_1,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_event_synchronize(struct ailo_ggml_backend_event *argument_0,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_event_wait(struct ailo_ggml_backend *argument_0,
                             struct ailo_ggml_backend_event *argument_1,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_free(struct ailo_ggml_backend *argument_0,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_get_alignment(struct ailo_ggml_backend *argument_0,
                                size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_get_default_buffer_type(
    struct ailo_ggml_backend *argument_0,
    struct ailo_ggml_backend_buffer_type **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_get_device(struct ailo_ggml_backend *argument_0,
                             struct ailo_ggml_backend_device **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_get_max_size(struct ailo_ggml_backend *argument_0,
                                           size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_graph_compute(
    struct ailo_ggml_backend *argument_0, struct ailo_ggml_cgraph *argument_1,
    enum ailo_ggml_status *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_graph_compute_async(
    struct ailo_ggml_backend *argument_0, struct ailo_ggml_cgraph *argument_1,
    enum ailo_ggml_status *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_graph_copy(
    struct ailo_ggml_backend *argument_0, struct ailo_ggml_cgraph *argument_1,
    struct ailo_ggml_backend_graph_copy **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_graph_copy_free(
    const struct ailo_ggml_backend_graph_copy *argument_0, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_graph_plan_compute(
    struct ailo_ggml_backend *argument_0, void *argument_1,
    enum ailo_ggml_status *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_graph_plan_create(struct ailo_ggml_backend *argument_0,
                                    struct ailo_ggml_cgraph *argument_1,
                                    void **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_graph_plan_free(struct ailo_ggml_backend *argument_0,
                                  void *argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_guid(struct ailo_ggml_backend *argument_0,
                                   ailo_ggml_guid **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_init_best(struct ailo_ggml_backend **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_init_by_name(const char *argument_0,
                                           const char *argument_1,
                                           struct ailo_ggml_backend **output,
                                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_init_by_type(
    enum ailo_ggml_backend_dev_type argument_0, const char *argument_1,
    struct ailo_ggml_backend **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_is_blas(struct ailo_ggml_backend *argument_0,
                                      ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_is_cann(struct ailo_ggml_backend *argument_0,
                                      ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_is_cpu(struct ailo_ggml_backend *argument_0,
                                     ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_is_cuda(struct ailo_ggml_backend *argument_0,
                                      ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_is_metal(struct ailo_ggml_backend *argument_0,
                                       ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_is_openvino(struct ailo_ggml_backend *argument_0,
                                          ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_is_rpc(struct ailo_ggml_backend *argument_0,
                                     ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_is_sycl(struct ailo_ggml_backend *argument_0,
                                      ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_is_vk(struct ailo_ggml_backend *argument_0,
                                    ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_is_zendnn(struct ailo_ggml_backend *argument_0,
                                        ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_load(const char *argument_0,
                                   struct ailo_ggml_backend_reg **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_load_all(ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_load_all_from_path(const char *argument_0,
                                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_meta_split_axis_name(
    enum ailo_ggml_backend_meta_split_axis argument_0, const char **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_metal_capture_next_compute(
    struct ailo_ggml_backend *argument_0, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_metal_init(struct ailo_ggml_backend **output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_metal_reg(struct ailo_ggml_backend_reg **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_metal_supports_family(struct ailo_ggml_backend *argument_0,
                                        int argument_1, ailo_bool *output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_name(struct ailo_ggml_backend *argument_0,
                                   const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_offload_op(struct ailo_ggml_backend *argument_0,
                             const struct ailo_ggml_tensor *argument_1,
                             ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_openvino_buffer_get_ctx_id(
    struct ailo_ggml_backend_buffer *argument_0, size_t *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_openvino_buffer_type(
    int argument_0, struct ailo_ggml_backend_buffer_type **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_openvino_get_device_count(int *output,
                                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_openvino_host_buffer_type(
    int argument_0, struct ailo_ggml_backend_buffer_type **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_openvino_init(int argument_0,
                                            struct ailo_ggml_backend **output,
                                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_openvino_reg(struct ailo_ggml_backend_reg **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_reg_by_name(const char *argument_0,
                                          struct ailo_ggml_backend_reg **output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_reg_count(size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_reg_dev_count(struct ailo_ggml_backend_reg *argument_0,
                                size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_reg_dev_get(
    struct ailo_ggml_backend_reg *argument_0, size_t argument_1,
    struct ailo_ggml_backend_device **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_reg_get(size_t argument_0,
                                      struct ailo_ggml_backend_reg **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_reg_get_proc_address(struct ailo_ggml_backend_reg *argument_0,
                                       const char *argument_1, void **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_reg_name(struct ailo_ggml_backend_reg *argument_0,
                                       const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_register(struct ailo_ggml_backend_reg *argument_0,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_rpc_add_server(const char *argument_0,
                                 struct ailo_ggml_backend_reg **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_rpc_buffer_type(const char *argument_0, ailo_u32 argument_1,
                                  struct ailo_ggml_backend_buffer_type **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_rpc_get_device_memory(const char *argument_0,
                                                    ailo_u32 argument_1,
                                                    size_t *argument_2,
                                                    size_t *argument_3,
                                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_rpc_init(const char *argument_0,
                                       ailo_u32 argument_1,
                                       struct ailo_ggml_backend **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_rpc_reg(struct ailo_ggml_backend_reg **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_rpc_start_server(
    const char *argument_0, const char *argument_1, size_t argument_2,
    size_t argument_3,
    struct ailo_storage_struct_ggml_backend_device *argument_4,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_sched_alloc_graph(struct ailo_ggml_backend_sched *argument_0,
                                    struct ailo_ggml_cgraph *argument_1,
                                    ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_sched_free(struct ailo_ggml_backend_sched *argument_0,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sched_get_backend(
    struct ailo_ggml_backend_sched *argument_0, int argument_1,
    struct ailo_ggml_backend **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sched_get_buffer_size(
    struct ailo_ggml_backend_sched *argument_0,
    struct ailo_ggml_backend *argument_1, size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sched_get_buffer_type(
    struct ailo_ggml_backend_sched *argument_0,
    struct ailo_ggml_backend *argument_1,
    struct ailo_ggml_backend_buffer_type **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sched_get_n_backends(
    struct ailo_ggml_backend_sched *argument_0, int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_sched_get_n_copies(struct ailo_ggml_backend_sched *argument_0,
                                     int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_sched_get_n_splits(struct ailo_ggml_backend_sched *argument_0,
                                     int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sched_get_tensor_backend(
    struct ailo_ggml_backend_sched *argument_0,
    struct ailo_ggml_tensor *argument_1, struct ailo_ggml_backend **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sched_graph_compute(
    struct ailo_ggml_backend_sched *argument_0,
    struct ailo_ggml_cgraph *argument_1, enum ailo_ggml_status *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sched_graph_compute_async(
    struct ailo_ggml_backend_sched *argument_0,
    struct ailo_ggml_cgraph *argument_1, enum ailo_ggml_status *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sched_new(
    struct ailo_storage_struct_ggml_backend *argument_0,
    struct ailo_storage_struct_ggml_backend_buffer_type *argument_1,
    int argument_2, size_t argument_3, ailo_bool argument_4,
    ailo_bool argument_5, struct ailo_ggml_backend_sched **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_sched_reserve(struct ailo_ggml_backend_sched *argument_0,
                                struct ailo_ggml_cgraph *argument_1,
                                ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_sched_reserve_size(struct ailo_ggml_backend_sched *argument_0,
                                     struct ailo_ggml_cgraph *argument_1,
                                     size_t *argument_2, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_sched_reset(struct ailo_ggml_backend_sched *argument_0,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sched_set_tensor_backend(
    struct ailo_ggml_backend_sched *argument_0,
    struct ailo_ggml_tensor *argument_1, struct ailo_ggml_backend *argument_2,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_sched_split_graph(struct ailo_ggml_backend_sched *argument_0,
                                    struct ailo_ggml_cgraph *argument_1,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_sched_synchronize(struct ailo_ggml_backend_sched *argument_0,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_supports_buft(
    struct ailo_ggml_backend *argument_0,
    struct ailo_ggml_backend_buffer_type *argument_1, ailo_bool *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_supports_op(struct ailo_ggml_backend *argument_0,
                              const struct ailo_ggml_tensor *argument_1,
                              ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_buffer_type(
    int argument_0, struct ailo_ggml_backend_buffer_type **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_comm_allreduce_tensor(
    void *argument_0, struct ailo_storage_struct_ggml_tensor *argument_1,
    ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_comm_free(void *argument_0,
                                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_comm_init(
    struct ailo_storage_struct_ggml_backend *argument_0, size_t argument_1,
    void **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_get_device_count(int *output,
                                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_get_device_description(int argument_0,
                                                          char *argument_1,
                                                          size_t argument_2,
                                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_get_device_memory(int argument_0,
                                                     size_t *argument_1,
                                                     size_t *argument_2,
                                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_get_gpu_list(int *argument_0, int argument_1,
                                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_host_buffer_type(
    struct ailo_ggml_backend_buffer_type **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_init(int argument_0,
                                        struct ailo_ggml_backend **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_print_sycl_devices(ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_reg(struct ailo_ggml_backend_reg **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_sycl_split_buffer_type(
    int argument_0, const float *argument_1,
    struct ailo_ggml_backend_buffer_type **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_synchronize(struct ailo_ggml_backend *argument_0,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_tensor_alloc(struct ailo_ggml_backend_buffer *argument_0,
                               struct ailo_ggml_tensor *argument_1,
                               void *argument_2, enum ailo_ggml_status *output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_tensor_copy(const struct ailo_ggml_tensor *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_tensor_copy_async(
    struct ailo_ggml_backend *argument_0, struct ailo_ggml_backend *argument_1,
    const struct ailo_ggml_tensor *argument_2,
    struct ailo_ggml_tensor *argument_3, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_tensor_get(const struct ailo_ggml_tensor *argument_0,
                             void *argument_1, size_t argument_2,
                             size_t argument_3, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_tensor_get_2d(
    const struct ailo_ggml_tensor *argument_0, void *argument_1,
    size_t argument_2, size_t argument_3, size_t argument_4, size_t argument_5,
    size_t argument_6, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_tensor_get_2d_async(
    struct ailo_ggml_backend *argument_0,
    const struct ailo_ggml_tensor *argument_1, void *argument_2,
    size_t argument_3, size_t argument_4, size_t argument_5, size_t argument_6,
    size_t argument_7, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_tensor_get_async(struct ailo_ggml_backend *argument_0,
                                   const struct ailo_ggml_tensor *argument_1,
                                   void *argument_2, size_t argument_3,
                                   size_t argument_4, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_tensor_memset(struct ailo_ggml_tensor *argument_0,
                                            ailo_u8 argument_1,
                                            size_t argument_2,
                                            size_t argument_3,
                                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_tensor_set(struct ailo_ggml_tensor *argument_0,
                                         const void *argument_1,
                                         size_t argument_2, size_t argument_3,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_tensor_set_2d(
    struct ailo_ggml_tensor *argument_0, const void *argument_1,
    size_t argument_2, size_t argument_3, size_t argument_4, size_t argument_5,
    size_t argument_6, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_tensor_set_2d_async(
    struct ailo_ggml_backend *argument_0, struct ailo_ggml_tensor *argument_1,
    const void *argument_2, size_t argument_3, size_t argument_4,
    size_t argument_5, size_t argument_6, size_t argument_7, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_tensor_set_async(struct ailo_ggml_backend *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   const void *argument_2, size_t argument_3,
                                   size_t argument_4, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_unload(struct ailo_ggml_backend_reg *argument_0,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_view_init(struct ailo_ggml_tensor *argument_0,
                                        enum ailo_ggml_status *output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_virtgpu_reg(struct ailo_ggml_backend_reg **output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_vk_buffer_type(size_t argument_0,
                                 struct ailo_ggml_backend_buffer_type **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_vk_get_device_count(int *output,
                                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_vk_get_device_description(int argument_0,
                                                        char *argument_1,
                                                        size_t argument_2,
                                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_vk_get_device_memory(int argument_0,
                                                   size_t *argument_1,
                                                   size_t *argument_2,
                                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_vk_host_buffer_type(
    struct ailo_ggml_backend_buffer_type **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_vk_init(size_t argument_0,
                                      struct ailo_ggml_backend **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_vk_reg(struct ailo_ggml_backend_reg **output,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_webgpu_init(struct ailo_ggml_backend **output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_webgpu_reg(struct ailo_ggml_backend_reg **output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_zendnn_init(struct ailo_ggml_backend **output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_backend_zendnn_reg(struct ailo_ggml_backend_reg **output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_backend_zendnn_set_n_threads(struct ailo_ggml_backend *argument_0,
                                       int argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_bf16_to_fp32(const struct ailo_ggml_bf16_t *argument_0,
                                   float *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_bf16_to_fp32_row(const struct ailo_ggml_bf16_t *argument_0,
                           float *argument_1, ailo_i64 argument_2,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_blck_size(enum ailo_ggml_type argument_0,
                                ailo_i64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_build_backward_expand(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_cgraph *argument_1,
    struct ailo_storage_struct_ggml_tensor *argument_2, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_build_forward_expand(struct ailo_ggml_cgraph *argument_0,
                                           struct ailo_ggml_tensor *argument_1,
                                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_build_forward_order(struct ailo_ggml_cgraph *argument_0,
                                          struct ailo_ggml_tensor *argument_1,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_build_forward_select(
    struct ailo_ggml_cgraph *argument_0,
    struct ailo_storage_struct_ggml_tensor *argument_1, int argument_2,
    int argument_3, struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_can_repeat(const struct ailo_ggml_tensor *argument_0,
                                 const struct ailo_ggml_tensor *argument_1,
                                 ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cast(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           enum ailo_ggml_type argument_2,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_ceil(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_ceil_inplace(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_clamp(struct ailo_ggml_context *argument_0,
                            struct ailo_ggml_tensor *argument_1,
                            float argument_2, float argument_3,
                            struct ailo_ggml_tensor **output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_clamp_inplace(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    float argument_2, float argument_3,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_col2im_1d(struct ailo_ggml_context *argument_0,
                                struct ailo_ggml_tensor *argument_1,
                                int argument_2, int argument_3, int argument_4,
                                struct ailo_ggml_tensor **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_commit(const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_concat(struct ailo_ggml_context *argument_0,
                             struct ailo_ggml_tensor *argument_1,
                             struct ailo_ggml_tensor *argument_2,
                             int argument_3, struct ailo_ggml_tensor **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cont(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cont_1d(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              ailo_i64 argument_2,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cont_2d(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              ailo_i64 argument_2, ailo_i64 argument_3,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cont_3d(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              ailo_i64 argument_2, ailo_i64 argument_3,
                              ailo_i64 argument_4,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cont_4d(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              ailo_i64 argument_2, ailo_i64 argument_3,
                              ailo_i64 argument_4, ailo_i64 argument_5,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_1d(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              struct ailo_ggml_tensor *argument_2,
                              int argument_3, int argument_4, int argument_5,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_1d_dw(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 struct ailo_ggml_tensor *argument_2,
                                 int argument_3, int argument_4, int argument_5,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_1d_dw_ph(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    struct ailo_ggml_tensor *argument_2,
                                    int argument_3, int argument_4,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_1d_ph(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 struct ailo_ggml_tensor *argument_2,
                                 int argument_3, int argument_4,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_2d(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              struct ailo_ggml_tensor *argument_2,
                              int argument_3, int argument_4, int argument_5,
                              int argument_6, int argument_7, int argument_8,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_2d_direct(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, int argument_3, int argument_4,
    int argument_5, int argument_6, int argument_7, int argument_8,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_2d_dw(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 struct ailo_ggml_tensor *argument_2,
                                 int argument_3, int argument_4, int argument_5,
                                 int argument_6, int argument_7, int argument_8,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_2d_dw_direct(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, int argument_3, int argument_4,
    int argument_5, int argument_6, int argument_7, int argument_8,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_2d_s1_ph(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    struct ailo_ggml_tensor *argument_2,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_2d_sk_p0(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    struct ailo_ggml_tensor *argument_2,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_3d(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, ailo_i64 argument_3, int argument_4,
    int argument_5, int argument_6, int argument_7, int argument_8,
    int argument_9, int argument_10, int argument_11, int argument_12,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_3d_direct(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, int argument_3, int argument_4,
    int argument_5, int argument_6, int argument_7, int argument_8,
    int argument_9, int argument_10, int argument_11, int argument_12,
    int argument_13, int argument_14, struct ailo_ggml_tensor **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_transpose_1d(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, int argument_3, int argument_4,
    int argument_5, struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_conv_transpose_2d_p0(struct ailo_ggml_context *argument_0,
                                           struct ailo_ggml_tensor *argument_1,
                                           struct ailo_ggml_tensor *argument_2,
                                           int argument_3,
                                           struct ailo_ggml_tensor **output,
                                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cos(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cos_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_count_equal(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor *argument_2,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_cpu_bf16_to_fp32(const struct ailo_ggml_bf16_t *argument_0,
                           float *argument_1, ailo_i64 argument_2,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_fp16_to_fp32(const ailo_u16 *argument_0,
                                       float *argument_1, ailo_i64 argument_2,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_fp32_to_bf16(const float *argument_0,
                                       struct ailo_ggml_bf16_t *argument_1,
                                       ailo_i64 argument_2, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_fp32_to_fp16(const float *argument_0,
                                       ailo_u16 *argument_1,
                                       ailo_i64 argument_2, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_fp32_to_fp32(const float *argument_0,
                                       float *argument_1, ailo_i64 argument_2,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_fp32_to_i32(const float *argument_0,
                                      ailo_i32 *argument_1, ailo_i64 argument_2,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_get_rvv_vlen(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_get_sve_cnt(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_amx_int8(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_arm_fma(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_avx(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_avx2(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_avx512(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_avx512_bf16(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_avx512_vbmi(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_avx512_vnni(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_avx_vnni(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_bmi2(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_dotprod(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_f16c(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_fma(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_fp16_va(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_llamafile(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_matmul_int8(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_neon(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_riscv_v(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_sme(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_sme2(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_sse3(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_ssse3(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_sve(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_vsx(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_vxe(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_has_wasm_simd(int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpu_init(ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cpy(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor *argument_2,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cross_entropy_loss(struct ailo_ggml_context *argument_0,
                                         struct ailo_ggml_tensor *argument_1,
                                         struct ailo_ggml_tensor *argument_2,
                                         struct ailo_ggml_tensor **output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cross_entropy_loss_back(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cumsum(struct ailo_ggml_context *argument_0,
                             struct ailo_ggml_tensor *argument_1,
                             struct ailo_ggml_tensor **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cycles(ailo_i64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_cycles_per_ms(ailo_i64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_diag(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_diag_mask_inf(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    int argument_2,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_diag_mask_inf_inplace(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    int argument_2, struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_diag_mask_zero(struct ailo_ggml_context *argument_0,
                                     struct ailo_ggml_tensor *argument_1,
                                     int argument_2,
                                     struct ailo_ggml_tensor **output,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_diag_mask_zero_inplace(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    int argument_2, struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_div(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor *argument_2,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_div_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor *argument_2,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_dsv4_hc_comb(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor *argument_2,
                                   struct ailo_ggml_tensor *argument_3,
                                   float argument_4, ailo_i32 argument_5,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_dsv4_hc_post(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor *argument_2,
                                   struct ailo_ggml_tensor *argument_3,
                                   struct ailo_ggml_tensor *argument_4,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_dsv4_hc_pre(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor *argument_2,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_dsv4_hc_pre_gated(struct ailo_ggml_context *argument_0,
                                        struct ailo_ggml_tensor *argument_1,
                                        struct ailo_ggml_tensor *argument_2,
                                        float argument_3,
                                        struct ailo_ggml_tensor **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_dup(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_dup_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_dup_tensor(struct ailo_ggml_context *argument_0,
                                 const struct ailo_ggml_tensor *argument_1,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_element_size(const struct ailo_ggml_tensor *argument_0,
                                   size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_elu(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_elu_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_exp(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_exp_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_expm1(struct ailo_ggml_context *argument_0,
                            struct ailo_ggml_tensor *argument_1,
                            struct ailo_ggml_tensor **output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_expm1_inplace(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_fill(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           float argument_2, struct ailo_ggml_tensor **output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_fill_inplace(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   float argument_2,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_flash_attn_back(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    struct ailo_ggml_tensor *argument_4, ailo_bool argument_5,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_flash_attn_ext(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    struct ailo_ggml_tensor *argument_4, float argument_5, float argument_6,
    float argument_7, struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_flash_attn_ext_add_sinks(struct ailo_ggml_tensor *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_flash_attn_ext_get_prec(const struct ailo_ggml_tensor *argument_0,
                                  enum ailo_ggml_prec *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_flash_attn_ext_set_n_kv_max(struct ailo_ggml_tensor *argument_0,
                                      ailo_i32 argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_flash_attn_ext_set_prec(struct ailo_ggml_tensor *argument_0,
                                  enum ailo_ggml_prec argument_1,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_floor(struct ailo_ggml_context *argument_0,
                            struct ailo_ggml_tensor *argument_1,
                            struct ailo_ggml_tensor **output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_floor_inplace(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_fopen(const char *argument_0, const char *argument_1,
                            FILE **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_fp16_to_fp32(ailo_u16 argument_0, float *output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_fp16_to_fp32_row(const ailo_u16 *argument_0,
                                       float *argument_1, ailo_i64 argument_2,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_fp32_to_bf16(float argument_0,
                                   struct ailo_ggml_bf16_t **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_fp32_to_bf16_row(const float *argument_0,
                                       struct ailo_ggml_bf16_t *argument_1,
                                       ailo_i64 argument_2, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_fp32_to_bf16_row_ref(const float *argument_0,
                                           struct ailo_ggml_bf16_t *argument_1,
                                           ailo_i64 argument_2,
                                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_fp32_to_fp16(float argument_0, ailo_u16 *output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_fp32_to_fp16_row(const float *argument_0,
                                       ailo_u16 *argument_1,
                                       ailo_i64 argument_2, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_free(struct ailo_ggml_context *argument_0,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_ftype_to_ggml_type(enum ailo_ggml_ftype argument_0,
                                         enum ailo_ggml_type *output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gallocr_alloc_graph(struct ailo_ggml_gallocr *argument_0,
                                          struct ailo_ggml_cgraph *argument_1,
                                          ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gallocr_free(struct ailo_ggml_gallocr *argument_0,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_gallocr_get_buffer_size(struct ailo_ggml_gallocr *argument_0,
                                  int argument_1, size_t *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_gallocr_new(struct ailo_ggml_backend_buffer_type *argument_0,
                      struct ailo_ggml_gallocr **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gallocr_new_n(
    struct ailo_storage_struct_ggml_backend_buffer_type *argument_0,
    int argument_1, struct ailo_ggml_gallocr **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gallocr_reserve(struct ailo_ggml_gallocr *argument_0,
                                      struct ailo_ggml_cgraph *argument_1,
                                      ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gallocr_reserve_n(struct ailo_ggml_gallocr *argument_0,
                                        struct ailo_ggml_cgraph *argument_1,
                                        const int *argument_2,
                                        const int *argument_3,
                                        ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_gallocr_reserve_n_size(struct ailo_ggml_gallocr *argument_0,
                                 struct ailo_ggml_cgraph *argument_1,
                                 const int *argument_2, const int *argument_3,
                                 size_t *argument_4, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gated_delta_net(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    struct ailo_ggml_tensor *argument_4, struct ailo_ggml_tensor *argument_5,
    struct ailo_ggml_tensor *argument_6, ailo_i64 argument_7,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gated_linear_attn(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    struct ailo_ggml_tensor *argument_4, struct ailo_ggml_tensor *argument_5,
    float argument_6, struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_geglu(struct ailo_ggml_context *argument_0,
                            struct ailo_ggml_tensor *argument_1,
                            struct ailo_ggml_tensor **output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_geglu_erf(struct ailo_ggml_context *argument_0,
                                struct ailo_ggml_tensor *argument_1,
                                struct ailo_ggml_tensor **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_geglu_erf_split(struct ailo_ggml_context *argument_0,
                                      struct ailo_ggml_tensor *argument_1,
                                      struct ailo_ggml_tensor *argument_2,
                                      struct ailo_ggml_tensor **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_geglu_erf_swapped(struct ailo_ggml_context *argument_0,
                                        struct ailo_ggml_tensor *argument_1,
                                        struct ailo_ggml_tensor **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_geglu_quick(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_geglu_quick_split(struct ailo_ggml_context *argument_0,
                                        struct ailo_ggml_tensor *argument_1,
                                        struct ailo_ggml_tensor *argument_2,
                                        struct ailo_ggml_tensor **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_geglu_quick_swapped(struct ailo_ggml_context *argument_0,
                                          struct ailo_ggml_tensor *argument_1,
                                          struct ailo_ggml_tensor **output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_geglu_split(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor *argument_2,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_geglu_swapped(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gelu(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gelu_erf(struct ailo_ggml_context *argument_0,
                               struct ailo_ggml_tensor *argument_1,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gelu_erf_inplace(struct ailo_ggml_context *argument_0,
                                       struct ailo_ggml_tensor *argument_1,
                                       struct ailo_ggml_tensor **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gelu_inplace(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gelu_quick(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_gelu_quick_inplace(struct ailo_ggml_context *argument_0,
                                         struct ailo_ggml_tensor *argument_1,
                                         struct ailo_ggml_tensor **output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_data(const struct ailo_ggml_tensor *argument_0,
                               void **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_data_f32(const struct ailo_ggml_tensor *argument_0,
                                   float **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_f32_1d(const struct ailo_ggml_tensor *argument_0,
                                 int argument_1, float *output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_f32_nd(const struct ailo_ggml_tensor *argument_0,
                                 int argument_1, int argument_2, int argument_3,
                                 int argument_4, float *output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_get_first_tensor(const struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_glu_op(const struct ailo_ggml_tensor *argument_0,
                                 enum ailo_ggml_glu_op *output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_i32_1d(const struct ailo_ggml_tensor *argument_0,
                                 int argument_1, ailo_i32 *output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_i32_nd(const struct ailo_ggml_tensor *argument_0,
                                 int argument_1, int argument_2, int argument_3,
                                 int argument_4, ailo_i32 *output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_get_max_tensor_size(const struct ailo_ggml_context *argument_0,
                              size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_mem_buffer(const struct ailo_ggml_context *argument_0,
                                     void **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_mem_size(const struct ailo_ggml_context *argument_0,
                                   size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_name(const struct ailo_ggml_tensor *argument_0,
                               const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_get_next_tensor(const struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_no_alloc(struct ailo_ggml_context *argument_0,
                                   ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_rel_pos(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  int argument_2, int argument_3,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_rows(struct ailo_ggml_context *argument_0,
                               struct ailo_ggml_tensor *argument_1,
                               struct ailo_ggml_tensor *argument_2,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_rows_back(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    struct ailo_ggml_tensor *argument_2,
                                    struct ailo_ggml_tensor *argument_3,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_tensor(struct ailo_ggml_context *argument_0,
                                 const char *argument_1,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_get_type_traits(enum ailo_ggml_type argument_0,
                          const struct ailo_ggml_type_traits **output,
                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_get_type_traits_cpu(enum ailo_ggml_type argument_0,
                              const struct ailo_ggml_type_traits_cpu **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_get_unary_op(const struct ailo_ggml_tensor *argument_0,
                                   enum ailo_ggml_unary_op *output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_glu(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          enum ailo_ggml_glu_op argument_2,
                          ailo_bool argument_3,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_glu_op_name(enum ailo_ggml_glu_op argument_0,
                                  const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_glu_split(struct ailo_ggml_context *argument_0,
                                struct ailo_ggml_tensor *argument_1,
                                struct ailo_ggml_tensor *argument_2,
                                enum ailo_ggml_glu_op argument_3,
                                struct ailo_ggml_tensor **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_add_node(struct ailo_ggml_cgraph *argument_0,
                                     struct ailo_ggml_tensor *argument_1,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_clear(struct ailo_ggml_cgraph *argument_0,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_compute(struct ailo_ggml_cgraph *argument_0,
                                    struct ailo_ggml_cplan *argument_1,
                                    enum ailo_ggml_status *output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_compute_with_ctx(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_cgraph *argument_1,
    int argument_2, enum ailo_ggml_status *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_cpy(struct ailo_ggml_cgraph *argument_0,
                                struct ailo_ggml_cgraph *argument_1,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_dump_dot(const struct ailo_ggml_cgraph *argument_0,
                                     const struct ailo_ggml_cgraph *argument_1,
                                     const char *argument_2, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_dup(struct ailo_ggml_context *argument_0,
                                struct ailo_ggml_cgraph *argument_1,
                                ailo_bool argument_2,
                                struct ailo_ggml_cgraph **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_get_grad(const struct ailo_ggml_cgraph *argument_0,
                                     const struct ailo_ggml_tensor *argument_1,
                                     struct ailo_ggml_tensor **output,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_graph_get_grad_acc(const struct ailo_ggml_cgraph *argument_0,
                             const struct ailo_ggml_tensor *argument_1,
                             struct ailo_ggml_tensor **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_graph_get_tensor(const struct ailo_ggml_cgraph *argument_0,
                           const char *argument_1,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_n_nodes(struct ailo_ggml_cgraph *argument_0,
                                    int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_node(struct ailo_ggml_cgraph *argument_0,
                                 int argument_1,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_graph_nodes(struct ailo_ggml_cgraph *argument_0,
                      struct ailo_storage_struct_ggml_tensor **output,
                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_overhead(size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_overhead_custom(size_t argument_0,
                                            ailo_bool argument_1,
                                            size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_plan(const struct ailo_ggml_cgraph *argument_0,
                                 int argument_1,
                                 struct ailo_ggml_threadpool *argument_2,
                                 struct ailo_ggml_cplan **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_print(const struct ailo_ggml_cgraph *argument_0,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_reset(struct ailo_ggml_cgraph *argument_0,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_graph_size(struct ailo_ggml_cgraph *argument_0,
                                 int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_group_norm(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 int argument_2, float argument_3,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_group_norm_inplace(struct ailo_ggml_context *argument_0,
                                         struct ailo_ggml_tensor *argument_1,
                                         int argument_2, float argument_3,
                                         struct ailo_ggml_tensor **output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_guid_matches(ailo_ggml_guid *argument_0,
                                   ailo_ggml_guid *argument_1,
                                   ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_hardsigmoid(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_hardswish(struct ailo_ggml_context *argument_0,
                                struct ailo_ggml_tensor *argument_1,
                                struct ailo_ggml_tensor **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_im2col(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, int argument_3, int argument_4,
    int argument_5, int argument_6, int argument_7, int argument_8,
    ailo_bool argument_9, enum ailo_ggml_type argument_10,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_im2col_3d(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, ailo_i64 argument_3, int argument_4,
    int argument_5, int argument_6, int argument_7, int argument_8,
    int argument_9, int argument_10, int argument_11, int argument_12,
    enum ailo_ggml_type argument_13, struct ailo_ggml_tensor **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_im2col_back(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2,
    struct ailo_storage_int64_t *argument_3, int argument_4, int argument_5,
    int argument_6, int argument_7, int argument_8, int argument_9,
    ailo_bool argument_10, struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_init(const struct ailo_ggml_init_params *argument_0,
                           struct ailo_ggml_context **output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_interpolate(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  ailo_i64 argument_2, ailo_i64 argument_3,
                                  ailo_i64 argument_4, ailo_i64 argument_5,
                                  ailo_u32 argument_6,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_3d(const struct ailo_ggml_tensor *argument_0,
                            ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_contiguous(const struct ailo_ggml_tensor *argument_0,
                                    ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_contiguous_0(const struct ailo_ggml_tensor *argument_0,
                                      ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_contiguous_1(const struct ailo_ggml_tensor *argument_0,
                                      ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_contiguous_2(const struct ailo_ggml_tensor *argument_0,
                                      ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_is_contiguous_channels(const struct ailo_ggml_tensor *argument_0,
                                 ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_is_contiguous_rows(const struct ailo_ggml_tensor *argument_0,
                             ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_is_contiguous_to_1(const struct ailo_ggml_tensor *argument_0,
                             ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_is_contiguous_to_2(const struct ailo_ggml_tensor *argument_0,
                             ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_is_contiguous_to_3(const struct ailo_ggml_tensor *argument_0,
                             ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_is_contiguously_allocated(const struct ailo_ggml_tensor *argument_0,
                                    ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_empty(const struct ailo_ggml_tensor *argument_0,
                               ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_matrix(const struct ailo_ggml_tensor *argument_0,
                                ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_numa(ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_permuted(const struct ailo_ggml_tensor *argument_0,
                                  ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_quantized(enum ailo_ggml_type argument_0,
                                   ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_scalar(const struct ailo_ggml_tensor *argument_0,
                                ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_transposed(const struct ailo_ggml_tensor *argument_0,
                                    ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_vector(const struct ailo_ggml_tensor *argument_0,
                                ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_is_view(const struct ailo_ggml_tensor *argument_0,
                              ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_l2_norm(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              float argument_2,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_l2_norm_inplace(struct ailo_ggml_context *argument_0,
                                      struct ailo_ggml_tensor *argument_1,
                                      float argument_2,
                                      struct ailo_ggml_tensor **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_leaky_relu(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 float argument_2, ailo_bool argument_3,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_lightning_indexer(struct ailo_ggml_context *argument_0,
                                        struct ailo_ggml_tensor *argument_1,
                                        struct ailo_ggml_tensor *argument_2,
                                        struct ailo_ggml_tensor *argument_3,
                                        struct ailo_ggml_tensor *argument_4,
                                        struct ailo_ggml_tensor **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_log(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_log_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_mean(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_mul(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor *argument_2,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_mul_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor *argument_2,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_mul_mat(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              struct ailo_ggml_tensor *argument_2,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_mul_mat_id(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 struct ailo_ggml_tensor *argument_2,
                                 struct ailo_ggml_tensor *argument_3,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_mul_mat_set_hint(struct ailo_ggml_tensor *argument_0,
                                       enum ailo_ggml_op_hint argument_1,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_mul_mat_set_prec(struct ailo_ggml_tensor *argument_0,
                                       enum ailo_ggml_prec argument_1,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_n_dims(const struct ailo_ggml_tensor *argument_0,
                             int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_nbytes(const struct ailo_ggml_tensor *argument_0,
                             size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_nbytes_pad(const struct ailo_ggml_tensor *argument_0,
                                 size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_neg(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_neg_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_nelements(const struct ailo_ggml_tensor *argument_0,
                                ailo_i64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_new_buffer(struct ailo_ggml_context *argument_0,
                                 size_t argument_1, void **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_new_f32(struct ailo_ggml_context *argument_0,
                              float argument_1,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_new_graph(struct ailo_ggml_context *argument_0,
                                struct ailo_ggml_cgraph **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_new_graph_custom(struct ailo_ggml_context *argument_0,
                                       size_t argument_1, ailo_bool argument_2,
                                       struct ailo_ggml_cgraph **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_new_i32(struct ailo_ggml_context *argument_0,
                              ailo_i32 argument_1,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_new_tensor(struct ailo_ggml_context *argument_0,
                                 enum ailo_ggml_type argument_1, int argument_2,
                                 const struct ailo_storage_int64_t *argument_3,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_new_tensor_1d(struct ailo_ggml_context *argument_0,
                                    enum ailo_ggml_type argument_1,
                                    ailo_i64 argument_2,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_new_tensor_2d(struct ailo_ggml_context *argument_0,
                                    enum ailo_ggml_type argument_1,
                                    ailo_i64 argument_2, ailo_i64 argument_3,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_new_tensor_3d(struct ailo_ggml_context *argument_0,
                                    enum ailo_ggml_type argument_1,
                                    ailo_i64 argument_2, ailo_i64 argument_3,
                                    ailo_i64 argument_4,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_new_tensor_4d(struct ailo_ggml_context *argument_0,
                                    enum ailo_ggml_type argument_1,
                                    ailo_i64 argument_2, ailo_i64 argument_3,
                                    ailo_i64 argument_4, ailo_i64 argument_5,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_norm(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           float argument_2, struct ailo_ggml_tensor **output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_norm_inplace(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   float argument_2,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_nrows(const struct ailo_ggml_tensor *argument_0,
                            ailo_i64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_numa_init(enum ailo_ggml_numa_strategy argument_0,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_op_desc(const struct ailo_ggml_tensor *argument_0,
                              const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_op_name(enum ailo_ggml_op argument_0, const char **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_op_symbol(enum ailo_ggml_op argument_0,
                                const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_alloc(struct ailo_ggml_opt_context *argument_0,
                                ailo_bool argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_opt_context_optimizer_type(struct ailo_ggml_opt_context *argument_0,
                                     enum ailo_ggml_opt_optimizer_type *output,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_dataset_data(struct ailo_ggml_opt_dataset *argument_0,
                                       struct ailo_ggml_tensor **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_dataset_free(struct ailo_ggml_opt_dataset *argument_0,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_opt_dataset_get_batch(struct ailo_ggml_opt_dataset *argument_0,
                                struct ailo_ggml_tensor *argument_1,
                                struct ailo_ggml_tensor *argument_2,
                                ailo_i64 argument_3, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_opt_dataset_get_batch_host(struct ailo_ggml_opt_dataset *argument_0,
                                     void *argument_1, size_t argument_2,
                                     void *argument_3, ailo_i64 argument_4,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_dataset_init(enum ailo_ggml_type argument_0,
                                       enum ailo_ggml_type argument_1,
                                       ailo_i64 argument_2, ailo_i64 argument_3,
                                       ailo_i64 argument_4, ailo_i64 argument_5,
                                       struct ailo_ggml_opt_dataset **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_opt_dataset_labels(struct ailo_ggml_opt_dataset *argument_0,
                             struct ailo_ggml_tensor **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_opt_dataset_ndata(struct ailo_ggml_opt_dataset *argument_0,
                            ailo_i64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_opt_dataset_shuffle(struct ailo_ggml_opt_context *argument_0,
                              struct ailo_ggml_opt_dataset *argument_1,
                              ailo_i64 argument_2, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_opt_default_params(struct ailo_ggml_backend_sched *argument_0,
                             enum ailo_ggml_opt_loss_type argument_1,
                             struct ailo_ggml_opt_params **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_epoch_callback_progress_bar(
    ailo_bool argument_0, struct ailo_ggml_opt_context *argument_1,
    struct ailo_ggml_opt_dataset *argument_2,
    struct ailo_ggml_opt_result *argument_3, ailo_i64 argument_4,
    ailo_i64 argument_5, ailo_i64 argument_6, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_eval(struct ailo_ggml_opt_context *argument_0,
                               struct ailo_ggml_opt_result *argument_1,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_free(struct ailo_ggml_opt_context *argument_0,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_get_constant_optimizer_params(
    void *argument_0, struct ailo_ggml_opt_optimizer_params **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_get_default_optimizer_params(
    void *argument_0, struct ailo_ggml_opt_optimizer_params **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_grad_acc(struct ailo_ggml_opt_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_init(const struct ailo_ggml_opt_params *argument_0,
                               struct ailo_ggml_opt_context **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_inputs(struct ailo_ggml_opt_context *argument_0,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_labels(struct ailo_ggml_opt_context *argument_0,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_loss(struct ailo_ggml_opt_context *argument_0,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_ncorrect(struct ailo_ggml_opt_context *argument_0,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_opt_optimizer_name(enum ailo_ggml_opt_optimizer_type argument_0,
                             const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_outputs(struct ailo_ggml_opt_context *argument_0,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_pred(struct ailo_ggml_opt_context *argument_0,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_prepare_alloc(
    struct ailo_ggml_opt_context *argument_0,
    struct ailo_ggml_context *argument_1, struct ailo_ggml_cgraph *argument_2,
    struct ailo_ggml_tensor *argument_3, struct ailo_ggml_tensor *argument_4,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_reset(struct ailo_ggml_opt_context *argument_0,
                                ailo_bool argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_opt_result_accuracy(struct ailo_ggml_opt_result *argument_0,
                              double *argument_1, double *argument_2,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_result_free(struct ailo_ggml_opt_result *argument_0,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_result_init(struct ailo_ggml_opt_result **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_result_loss(struct ailo_ggml_opt_result *argument_0,
                                      double *argument_1, double *argument_2,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_result_ndata(struct ailo_ggml_opt_result *argument_0,
                                       struct ailo_storage_int64_t *argument_1,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_result_pred(struct ailo_ggml_opt_result *argument_0,
                                      ailo_i32 *argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_result_reset(struct ailo_ggml_opt_result *argument_0,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_opt_static_graphs(struct ailo_ggml_opt_context *argument_0,
                            ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_step_adamw(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    struct ailo_ggml_tensor *argument_4, struct ailo_ggml_tensor *argument_5,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_opt_step_sgd(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor *argument_2,
                                   struct ailo_ggml_tensor *argument_3,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_out_prod(struct ailo_ggml_context *argument_0,
                               struct ailo_ggml_tensor *argument_1,
                               struct ailo_ggml_tensor *argument_2,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_pad(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1, int argument_2,
                          int argument_3, int argument_4, int argument_5,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_pad_circular(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   int argument_2, int argument_3,
                                   int argument_4, int argument_5,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_pad_ext(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              int argument_2, int argument_3, int argument_4,
                              int argument_5, int argument_6, int argument_7,
                              int argument_8, int argument_9,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_pad_ext_circular(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    int argument_2, int argument_3, int argument_4, int argument_5,
    int argument_6, int argument_7, int argument_8, int argument_9,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_pad_reflect_1d(struct ailo_ggml_context *argument_0,
                                     struct ailo_ggml_tensor *argument_1,
                                     int argument_2, int argument_3,
                                     struct ailo_ggml_tensor **output,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_permute(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              int argument_2, int argument_3, int argument_4,
                              int argument_5, struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_pool_1d(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              enum ailo_ggml_op_pool argument_2, int argument_3,
                              int argument_4, int argument_5,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_pool_2d(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              enum ailo_ggml_op_pool argument_2, int argument_3,
                              int argument_4, int argument_5, int argument_6,
                              float argument_7, float argument_8,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_pool_2d_back(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, enum ailo_ggml_op_pool argument_3,
    int argument_4, int argument_5, int argument_6, int argument_7,
    float argument_8, float argument_9, struct ailo_ggml_tensor **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_prec_set_acc(struct ailo_ggml_tensor *argument_0,
                                   enum ailo_ggml_prec argument_1,
                                   ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_prec_set_src(struct ailo_ggml_tensor *argument_0,
                                   enum ailo_ggml_prec argument_1,
                                   int argument_2, ailo_bool *output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_print_object(const struct ailo_ggml_object *argument_0,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_print_objects(const struct ailo_ggml_context *argument_0,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_quantize_chunk(enum ailo_ggml_type argument_0,
                                     const float *argument_1, void *argument_2,
                                     ailo_i64 argument_3, ailo_i64 argument_4,
                                     ailo_i64 argument_5,
                                     const float *argument_6, size_t *output,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_quantize_free(ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_quantize_init(enum ailo_ggml_type argument_0,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_quantize_requires_imatrix(enum ailo_ggml_type argument_0,
                                                ailo_bool *output,
                                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_reglu(struct ailo_ggml_context *argument_0,
                            struct ailo_ggml_tensor *argument_1,
                            struct ailo_ggml_tensor **output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_reglu_split(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor *argument_2,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_reglu_swapped(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_relu(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_relu_inplace(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_repeat(struct ailo_ggml_context *argument_0,
                             struct ailo_ggml_tensor *argument_1,
                             struct ailo_ggml_tensor *argument_2,
                             struct ailo_ggml_tensor **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_repeat_4d(struct ailo_ggml_context *argument_0,
                                struct ailo_ggml_tensor *argument_1,
                                ailo_i64 argument_2, ailo_i64 argument_3,
                                ailo_i64 argument_4, ailo_i64 argument_5,
                                struct ailo_ggml_tensor **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_repeat_back(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor *argument_2,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_reset(struct ailo_ggml_context *argument_0,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_reshape(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              struct ailo_ggml_tensor *argument_2,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_reshape_1d(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 ailo_i64 argument_2,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_reshape_2d(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 ailo_i64 argument_2, ailo_i64 argument_3,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_reshape_3d(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 ailo_i64 argument_2, ailo_i64 argument_3,
                                 ailo_i64 argument_4,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_reshape_4d(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 ailo_i64 argument_2, ailo_i64 argument_3,
                                 ailo_i64 argument_4, ailo_i64 argument_5,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rms_norm(struct ailo_ggml_context *argument_0,
                               struct ailo_ggml_tensor *argument_1,
                               float argument_2,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rms_norm_back(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    struct ailo_ggml_tensor *argument_2,
                                    float argument_3,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rms_norm_inplace(struct ailo_ggml_context *argument_0,
                                       struct ailo_ggml_tensor *argument_1,
                                       float argument_2,
                                       struct ailo_ggml_tensor **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_roll(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1, int argument_2,
                           int argument_3, int argument_4, int argument_5,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rope(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           struct ailo_ggml_tensor *argument_2, int argument_3,
                           int argument_4, struct ailo_ggml_tensor **output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rope_custom(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, int argument_3, int argument_4,
    int argument_5, float argument_6, float argument_7, float argument_8,
    float argument_9, float argument_10, float argument_11,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rope_custom_inplace(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, int argument_3, int argument_4,
    int argument_5, float argument_6, float argument_7, float argument_8,
    float argument_9, float argument_10, float argument_11,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rope_ext(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    int argument_4, int argument_5, int argument_6, float argument_7,
    float argument_8, float argument_9, float argument_10, float argument_11,
    float argument_12, struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rope_ext_back(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    int argument_4, int argument_5, int argument_6, float argument_7,
    float argument_8, float argument_9, float argument_10, float argument_11,
    float argument_12, struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rope_ext_inplace(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    int argument_4, int argument_5, int argument_6, float argument_7,
    float argument_8, float argument_9, float argument_10, float argument_11,
    float argument_12, struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rope_inplace(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor *argument_2,
                                   int argument_3, int argument_4,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rope_multi(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    int argument_4, int *argument_5, int argument_6, int argument_7,
    float argument_8, float argument_9, float argument_10, float argument_11,
    float argument_12, float argument_13, struct ailo_ggml_tensor **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rope_multi_back(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    int argument_4, int *argument_5, int argument_6, int argument_7,
    float argument_8, float argument_9, float argument_10, float argument_11,
    float argument_12, float argument_13, struct ailo_ggml_tensor **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rope_multi_inplace(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    int argument_4, int *argument_5, int argument_6, int argument_7,
    float argument_8, float argument_9, float argument_10, float argument_11,
    float argument_12, float argument_13, struct ailo_ggml_tensor **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rope_set_offset(struct ailo_ggml_tensor *argument_0,
                                      int argument_1,
                                      struct ailo_ggml_tensor **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rope_yarn_corr_dims(int argument_0, int argument_1,
                                          float argument_2, float argument_3,
                                          float argument_4, float *argument_5,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_round(struct ailo_ggml_context *argument_0,
                            struct ailo_ggml_tensor *argument_1,
                            struct ailo_ggml_tensor **output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_round_inplace(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_row_size(enum ailo_ggml_type argument_0,
                               ailo_i64 argument_1, size_t *output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rwkv_wkv6(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    struct ailo_ggml_tensor *argument_4, struct ailo_ggml_tensor *argument_5,
    struct ailo_ggml_tensor *argument_6, struct ailo_ggml_tensor **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_rwkv_wkv7(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    struct ailo_ggml_tensor *argument_4, struct ailo_ggml_tensor *argument_5,
    struct ailo_ggml_tensor *argument_6, struct ailo_ggml_tensor *argument_7,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_scale(struct ailo_ggml_context *argument_0,
                            struct ailo_ggml_tensor *argument_1,
                            float argument_2, struct ailo_ggml_tensor **output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_scale_bias(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 float argument_2, float argument_3,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_scale_bias_inplace(struct ailo_ggml_context *argument_0,
                                         struct ailo_ggml_tensor *argument_1,
                                         float argument_2, float argument_3,
                                         struct ailo_ggml_tensor **output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_scale_inplace(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    float argument_2,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor *argument_2,
                          size_t argument_3, size_t argument_4,
                          size_t argument_5, size_t argument_6,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_1d(struct ailo_ggml_context *argument_0,
                             struct ailo_ggml_tensor *argument_1,
                             struct ailo_ggml_tensor *argument_2,
                             size_t argument_3,
                             struct ailo_ggml_tensor **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_1d_inplace(struct ailo_ggml_context *argument_0,
                                     struct ailo_ggml_tensor *argument_1,
                                     struct ailo_ggml_tensor *argument_2,
                                     size_t argument_3,
                                     struct ailo_ggml_tensor **output,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_2d(struct ailo_ggml_context *argument_0,
                             struct ailo_ggml_tensor *argument_1,
                             struct ailo_ggml_tensor *argument_2,
                             size_t argument_3, size_t argument_4,
                             struct ailo_ggml_tensor **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_2d_inplace(struct ailo_ggml_context *argument_0,
                                     struct ailo_ggml_tensor *argument_1,
                                     struct ailo_ggml_tensor *argument_2,
                                     size_t argument_3, size_t argument_4,
                                     struct ailo_ggml_tensor **output,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_f32(struct ailo_ggml_tensor *argument_0,
                              float argument_1,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_f32_1d(const struct ailo_ggml_tensor *argument_0,
                                 int argument_1, float argument_2,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_f32_nd(const struct ailo_ggml_tensor *argument_0,
                                 int argument_1, int argument_2, int argument_3,
                                 int argument_4, float argument_5,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_i32(struct ailo_ggml_tensor *argument_0,
                              ailo_i32 argument_1,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_i32_1d(const struct ailo_ggml_tensor *argument_0,
                                 int argument_1, ailo_i32 argument_2,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_i32_nd(const struct ailo_ggml_tensor *argument_0,
                                 int argument_1, int argument_2, int argument_3,
                                 int argument_4, ailo_i32 argument_5,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor *argument_2,
                                  size_t argument_3, size_t argument_4,
                                  size_t argument_5, size_t argument_6,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_input(struct ailo_ggml_tensor *argument_0,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_loss(struct ailo_ggml_tensor *argument_0,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_name(struct ailo_ggml_tensor *argument_0,
                               const char *argument_1,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_no_alloc(struct ailo_ggml_context *argument_0,
                                   ailo_bool argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_output(struct ailo_ggml_tensor *argument_0,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_param(struct ailo_ggml_tensor *argument_0,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_rows(struct ailo_ggml_context *argument_0,
                               struct ailo_ggml_tensor *argument_1,
                               struct ailo_ggml_tensor *argument_2,
                               struct ailo_ggml_tensor *argument_3,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_set_zero(struct ailo_ggml_tensor *argument_0,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sgn(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sgn_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sigmoid(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sigmoid_inplace(struct ailo_ggml_context *argument_0,
                                      struct ailo_ggml_tensor *argument_1,
                                      struct ailo_ggml_tensor **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_silu(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_silu_back(struct ailo_ggml_context *argument_0,
                                struct ailo_ggml_tensor *argument_1,
                                struct ailo_ggml_tensor *argument_2,
                                struct ailo_ggml_tensor **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_silu_inplace(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sin(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sin_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_soft_max(struct ailo_ggml_context *argument_0,
                               struct ailo_ggml_tensor *argument_1,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_soft_max_add_sinks(struct ailo_ggml_tensor *argument_0,
                                         struct ailo_ggml_tensor *argument_1,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_soft_max_ext(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor *argument_2,
                                   float argument_3, float argument_4,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_soft_max_ext_back(struct ailo_ggml_context *argument_0,
                                        struct ailo_ggml_tensor *argument_1,
                                        struct ailo_ggml_tensor *argument_2,
                                        float argument_3, float argument_4,
                                        struct ailo_ggml_tensor **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_soft_max_ext_back_inplace(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, float argument_3, float argument_4,
    struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_soft_max_ext_inplace(struct ailo_ggml_context *argument_0,
                                           struct ailo_ggml_tensor *argument_1,
                                           struct ailo_ggml_tensor *argument_2,
                                           float argument_3, float argument_4,
                                           struct ailo_ggml_tensor **output,
                                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_soft_max_inplace(struct ailo_ggml_context *argument_0,
                                       struct ailo_ggml_tensor *argument_1,
                                       struct ailo_ggml_tensor **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_softplus(struct ailo_ggml_context *argument_0,
                               struct ailo_ggml_tensor *argument_1,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_softplus_inplace(struct ailo_ggml_context *argument_0,
                                       struct ailo_ggml_tensor *argument_1,
                                       struct ailo_ggml_tensor **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_solve_tri(struct ailo_ggml_context *argument_0,
                                struct ailo_ggml_tensor *argument_1,
                                struct ailo_ggml_tensor *argument_2,
                                ailo_bool argument_3, ailo_bool argument_4,
                                ailo_bool argument_5,
                                struct ailo_ggml_tensor **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sqr(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sqr_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sqrt(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sqrt_inplace(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_ssm_conv(struct ailo_ggml_context *argument_0,
                               struct ailo_ggml_tensor *argument_1,
                               struct ailo_ggml_tensor *argument_2,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_ssm_scan(
    struct ailo_ggml_context *argument_0, struct ailo_ggml_tensor *argument_1,
    struct ailo_ggml_tensor *argument_2, struct ailo_ggml_tensor *argument_3,
    struct ailo_ggml_tensor *argument_4, struct ailo_ggml_tensor *argument_5,
    struct ailo_ggml_tensor *argument_6, struct ailo_ggml_tensor *argument_7,
    ailo_i64 argument_8, struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_status_to_string(enum ailo_ggml_status argument_0,
                                       const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_step(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_step_inplace(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sub(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor *argument_2,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sub_inplace(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor *argument_2,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sum(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_sum_rows(struct ailo_ggml_context *argument_0,
                               struct ailo_ggml_tensor *argument_1,
                               struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_swiglu(struct ailo_ggml_context *argument_0,
                             struct ailo_ggml_tensor *argument_1,
                             struct ailo_ggml_tensor **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_swiglu_clamp(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor *argument_2,
                                   float argument_3,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_swiglu_oai(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 struct ailo_ggml_tensor *argument_2,
                                 float argument_3, float argument_4,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_swiglu_split(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor *argument_2,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_swiglu_swapped(struct ailo_ggml_context *argument_0,
                                     struct ailo_ggml_tensor *argument_1,
                                     struct ailo_ggml_tensor **output,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_tallocr_alloc(struct ailo_ggml_tallocr *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    enum ailo_ggml_status *output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_tallocr_new(struct ailo_ggml_backend_buffer *argument_0,
                                  struct ailo_ggml_tallocr **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_tanh(struct ailo_ggml_context *argument_0,
                           struct ailo_ggml_tensor *argument_1,
                           struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_tanh_inplace(struct ailo_ggml_context *argument_0,
                                   struct ailo_ggml_tensor *argument_1,
                                   struct ailo_ggml_tensor **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_tensor_overhead(size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_threadpool_free(struct ailo_ggml_threadpool *argument_0,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_threadpool_get_n_threads(struct ailo_ggml_threadpool *argument_0,
                                   int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_threadpool_new(struct ailo_ggml_threadpool_params *argument_0,
                         struct ailo_ggml_threadpool **output,
                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_threadpool_params_default(int argument_0,
                                    struct ailo_ggml_threadpool_params **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_ggml_threadpool_params_init(struct ailo_ggml_threadpool_params *argument_0,
                                 int argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_threadpool_params_match(
    const struct ailo_ggml_threadpool_params *argument_0,
    const struct ailo_ggml_threadpool_params *argument_1, ailo_bool *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_threadpool_pause(struct ailo_ggml_threadpool *argument_0,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_threadpool_resume(struct ailo_ggml_threadpool *argument_0,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_time_init(ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_time_ms(ailo_i64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_time_us(ailo_i64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_timestep_embedding(struct ailo_ggml_context *argument_0,
                                         struct ailo_ggml_tensor *argument_1,
                                         int argument_2, int argument_3,
                                         struct ailo_ggml_tensor **output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_top_k(struct ailo_ggml_context *argument_0,
                            struct ailo_ggml_tensor *argument_1, int argument_2,
                            struct ailo_ggml_tensor **output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_transpose(struct ailo_ggml_context *argument_0,
                                struct ailo_ggml_tensor *argument_1,
                                struct ailo_ggml_tensor **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_tri(struct ailo_ggml_context *argument_0,
                          struct ailo_ggml_tensor *argument_1,
                          enum ailo_ggml_tri_type argument_2,
                          struct ailo_ggml_tensor **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_trunc(struct ailo_ggml_context *argument_0,
                            struct ailo_ggml_tensor *argument_1,
                            struct ailo_ggml_tensor **output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_trunc_inplace(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_type_name(enum ailo_ggml_type argument_0,
                                const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_type_size(enum ailo_ggml_type argument_0, size_t *output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_type_sizef(enum ailo_ggml_type argument_0, double *output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_unary(struct ailo_ggml_context *argument_0,
                            struct ailo_ggml_tensor *argument_1,
                            enum ailo_ggml_unary_op argument_2,
                            struct ailo_ggml_tensor **output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_unary_inplace(struct ailo_ggml_context *argument_0,
                                    struct ailo_ggml_tensor *argument_1,
                                    enum ailo_ggml_unary_op argument_2,
                                    struct ailo_ggml_tensor **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_unary_op_name(enum ailo_ggml_unary_op argument_0,
                                    const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_unravel_index(const struct ailo_ggml_tensor *argument_0,
                                    ailo_i64 argument_1,
                                    struct ailo_storage_int64_t *argument_2,
                                    struct ailo_storage_int64_t *argument_3,
                                    struct ailo_storage_int64_t *argument_4,
                                    struct ailo_storage_int64_t *argument_5,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_upscale(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              int argument_2,
                              enum ailo_ggml_scale_mode argument_3,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_upscale_ext(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  int argument_2, int argument_3,
                                  int argument_4, int argument_5,
                                  enum ailo_ggml_scale_mode argument_6,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_used_mem(const struct ailo_ggml_context *argument_0,
                               size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_validate_row_data(enum ailo_ggml_type argument_0,
                                        const void *argument_1,
                                        size_t argument_2, ailo_bool *output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_version(const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_view_1d(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              ailo_i64 argument_2, size_t argument_3,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_view_2d(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              ailo_i64 argument_2, ailo_i64 argument_3,
                              size_t argument_4, size_t argument_5,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_view_3d(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              ailo_i64 argument_2, ailo_i64 argument_3,
                              ailo_i64 argument_4, size_t argument_5,
                              size_t argument_6, size_t argument_7,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_view_4d(struct ailo_ggml_context *argument_0,
                              struct ailo_ggml_tensor *argument_1,
                              ailo_i64 argument_2, ailo_i64 argument_3,
                              ailo_i64 argument_4, ailo_i64 argument_5,
                              size_t argument_6, size_t argument_7,
                              size_t argument_8, size_t argument_9,
                              struct ailo_ggml_tensor **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_view_tensor(struct ailo_ggml_context *argument_0,
                                  struct ailo_ggml_tensor *argument_1,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_win_part(struct ailo_ggml_context *argument_0,
                               struct ailo_ggml_tensor *argument_1,
                               int argument_2, struct ailo_ggml_tensor **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_win_unpart(struct ailo_ggml_context *argument_0,
                                 struct ailo_ggml_tensor *argument_1,
                                 int argument_2, int argument_3, int argument_4,
                                 struct ailo_ggml_tensor **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_ggml_xielu(struct ailo_ggml_context *argument_0,
                            struct ailo_ggml_tensor *argument_1,
                            float argument_2, float argument_3,
                            float argument_4, float argument_5,
                            struct ailo_ggml_tensor **output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_add_tensor(struct ailo_gguf_context *argument_0,
                                 const struct ailo_ggml_tensor *argument_1,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_find_key(const struct ailo_gguf_context *argument_0,
                               const char *argument_1, ailo_i64 *output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_find_tensor(const struct ailo_gguf_context *argument_0,
                                  const char *argument_1, ailo_i64 *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_free(struct ailo_gguf_context *argument_0,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_alignment(const struct ailo_gguf_context *argument_0,
                                    size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_arr_data(const struct ailo_gguf_context *argument_0,
                                   ailo_i64 argument_1, const void **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_arr_n(const struct ailo_gguf_context *argument_0,
                                ailo_i64 argument_1, size_t *output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_arr_str(const struct ailo_gguf_context *argument_0,
                                  ailo_i64 argument_1, size_t argument_2,
                                  const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_arr_type(const struct ailo_gguf_context *argument_0,
                                   ailo_i64 argument_1,
                                   enum ailo_gguf_type *output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_gguf_get_data_offset(const struct ailo_gguf_context *argument_0,
                          size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_key(const struct ailo_gguf_context *argument_0,
                              ailo_i64 argument_1, const char **output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_kv_type(const struct ailo_gguf_context *argument_0,
                                  ailo_i64 argument_1,
                                  enum ailo_gguf_type *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_meta_data(const struct ailo_gguf_context *argument_0,
                                    void *argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_meta_size(const struct ailo_gguf_context *argument_0,
                                    size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_n_kv(const struct ailo_gguf_context *argument_0,
                               ailo_i64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_n_tensors(const struct ailo_gguf_context *argument_0,
                                    ailo_i64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_gguf_get_tensor_name(const struct ailo_gguf_context *argument_0,
                          ailo_i64 argument_1, const char **output,
                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_tensor_ne(const struct ailo_gguf_context *argument_0,
                                    ailo_i64 argument_1,
                                    const struct ailo_storage_int64_t **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_gguf_get_tensor_offset(const struct ailo_gguf_context *argument_0,
                            ailo_i64 argument_1, size_t *output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_gguf_get_tensor_size(const struct ailo_gguf_context *argument_0,
                          ailo_i64 argument_1, size_t *output,
                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_gguf_get_tensor_type(const struct ailo_gguf_context *argument_0,
                          ailo_i64 argument_1, enum ailo_ggml_type *output,
                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_bool(const struct ailo_gguf_context *argument_0,
                                   ailo_i64 argument_1, ailo_bool *output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_data(const struct ailo_gguf_context *argument_0,
                                   ailo_i64 argument_1, const void **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_f32(const struct ailo_gguf_context *argument_0,
                                  ailo_i64 argument_1, float *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_f64(const struct ailo_gguf_context *argument_0,
                                  ailo_i64 argument_1, double *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_i16(const struct ailo_gguf_context *argument_0,
                                  ailo_i64 argument_1, ailo_i16 *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_i32(const struct ailo_gguf_context *argument_0,
                                  ailo_i64 argument_1, ailo_i32 *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_i64(const struct ailo_gguf_context *argument_0,
                                  ailo_i64 argument_1, ailo_i64 *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_i8(const struct ailo_gguf_context *argument_0,
                                 ailo_i64 argument_1, ailo_i8 *output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_str(const struct ailo_gguf_context *argument_0,
                                  ailo_i64 argument_1, const char **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_u16(const struct ailo_gguf_context *argument_0,
                                  ailo_i64 argument_1, ailo_u16 *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_u32(const struct ailo_gguf_context *argument_0,
                                  ailo_i64 argument_1, ailo_u32 *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_u64(const struct ailo_gguf_context *argument_0,
                                  ailo_i64 argument_1, ailo_u64 *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_val_u8(const struct ailo_gguf_context *argument_0,
                                 ailo_i64 argument_1, ailo_u8 *output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_get_version(const struct ailo_gguf_context *argument_0,
                                  ailo_u32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_init_empty(struct ailo_gguf_context **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_gguf_init_from_buffer(const void *argument_0, size_t argument_1,
                           const struct ailo_gguf_init_params *argument_2,
                           struct ailo_gguf_context **output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_gguf_init_from_file(const char *argument_0,
                         const struct ailo_gguf_init_params *argument_1,
                         struct ailo_gguf_context **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_init_from_file_ptr(
    FILE *argument_0, const struct ailo_gguf_init_params *argument_1,
    struct ailo_gguf_context **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_remove_key(struct ailo_gguf_context *argument_0,
                                 const char *argument_1, ailo_i64 *output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_arr_data(struct ailo_gguf_context *argument_0,
                                   const char *argument_1,
                                   enum ailo_gguf_type argument_2,
                                   const void *argument_3, size_t argument_4,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_arr_str(struct ailo_gguf_context *argument_0,
                                  const char *argument_1,
                                  const char **argument_2, size_t argument_3,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_kv(struct ailo_gguf_context *argument_0,
                             const struct ailo_gguf_context *argument_1,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_tensor_data(struct ailo_gguf_context *argument_0,
                                      const char *argument_1,
                                      const void *argument_2,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_tensor_type(struct ailo_gguf_context *argument_0,
                                      const char *argument_1,
                                      enum ailo_ggml_type argument_2,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_val_bool(struct ailo_gguf_context *argument_0,
                                   const char *argument_1, ailo_bool argument_2,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_val_f32(struct ailo_gguf_context *argument_0,
                                  const char *argument_1, float argument_2,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_val_f64(struct ailo_gguf_context *argument_0,
                                  const char *argument_1, double argument_2,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_val_i16(struct ailo_gguf_context *argument_0,
                                  const char *argument_1, ailo_i16 argument_2,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_val_i32(struct ailo_gguf_context *argument_0,
                                  const char *argument_1, ailo_i32 argument_2,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_val_i64(struct ailo_gguf_context *argument_0,
                                  const char *argument_1, ailo_i64 argument_2,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_val_i8(struct ailo_gguf_context *argument_0,
                                 const char *argument_1, ailo_i8 argument_2,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_val_str(struct ailo_gguf_context *argument_0,
                                  const char *argument_1,
                                  const char *argument_2, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_val_u16(struct ailo_gguf_context *argument_0,
                                  const char *argument_1, ailo_u16 argument_2,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_val_u32(struct ailo_gguf_context *argument_0,
                                  const char *argument_1, ailo_u32 argument_2,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_val_u64(struct ailo_gguf_context *argument_0,
                                  const char *argument_1, ailo_u64 argument_2,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_set_val_u8(struct ailo_gguf_context *argument_0,
                                 const char *argument_1, ailo_u8 argument_2,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_type_name(enum ailo_gguf_type argument_0,
                                const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_gguf_write_to_file(const struct ailo_gguf_context *argument_0,
                                    const char *argument_1,
                                    ailo_bool argument_2, ailo_bool *output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_gguf_write_to_file_ptr(const struct ailo_gguf_context *argument_0,
                            FILE *argument_1, ailo_bool argument_2,
                            ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_adapter_get_alora_invocation_tokens(
    const struct ailo_llama_adapter_lora *argument_0, const ailo_i32 **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_adapter_get_alora_n_invocation_tokens(
    const struct ailo_llama_adapter_lora *argument_0, ailo_u64 *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_adapter_lora_free(struct ailo_llama_adapter_lora *argument_0,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_adapter_lora_init(
    struct ailo_llama_model *argument_0, const char *argument_1,
    struct ailo_llama_adapter_lora **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_adapter_lora_init_from_file_ptr(
    struct ailo_llama_model *argument_0, FILE *argument_1,
    struct ailo_llama_adapter_lora **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_adapter_meta_count(const struct ailo_llama_adapter_lora *argument_0,
                              ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_adapter_meta_key_by_index(
    const struct ailo_llama_adapter_lora *argument_0, ailo_i32 argument_1,
    char *argument_2, size_t argument_3, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_adapter_meta_val_str(
    const struct ailo_llama_adapter_lora *argument_0, const char *argument_1,
    char *argument_2, size_t argument_3, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_adapter_meta_val_str_by_index(
    const struct ailo_llama_adapter_lora *argument_0, ailo_i32 argument_1,
    char *argument_2, size_t argument_3, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_add_bos_token(const struct ailo_llama_vocab *argument_0,
                                     ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_add_eos_token(const struct ailo_llama_vocab *argument_0,
                                     ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_attach_threadpool(struct ailo_llama_context *argument_0,
                             struct ailo_ggml_threadpool *argument_1,
                             struct ailo_ggml_threadpool *argument_2,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_backend_free(ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_backend_init(ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_batch_ext_add(struct ailo_llama_batch_ext *argument_0,
                                     ailo_i32 argument_1, ailo_i32 *output,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_batch_ext_add_embd(struct ailo_llama_batch_ext *argument_0,
                              ailo_i32 argument_1,
                              const struct ailo_llama_embd *argument_2,
                              ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_batch_ext_add_seq(struct ailo_llama_batch_ext *argument_0,
                             ailo_i32 argument_1, ailo_i32 argument_2,
                             ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_batch_ext_add_token(struct ailo_llama_batch_ext *argument_0,
                               ailo_i32 argument_1, ailo_i32 argument_2,
                               ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_batch_ext_clear(struct ailo_llama_batch_ext *argument_0,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_batch_ext_free(struct ailo_llama_batch_ext *argument_0,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_batch_ext_init(struct ailo_llama_context *argument_0,
                                      struct ailo_llama_batch_ext **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_batch_ext_set_embd_state(struct ailo_llama_batch_ext *argument_0,
                                    ailo_i32 argument_1,
                                    const struct ailo_llama_embd *argument_2,
                                    ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_batch_ext_set_embd_token(struct ailo_llama_batch_ext *argument_0,
                                    ailo_i32 argument_1,
                                    const struct ailo_llama_embd *argument_2,
                                    ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_batch_ext_set_output_embd(struct ailo_llama_batch_ext *argument_0,
                                     ailo_i32 argument_1, ailo_bool argument_2,
                                     ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_batch_ext_set_output_logits(
    struct ailo_llama_batch_ext *argument_0, ailo_i32 argument_1,
    ailo_bool argument_2, ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_batch_ext_set_pos(struct ailo_llama_batch_ext *argument_0,
                             ailo_i32 argument_1, const ailo_i32 *argument_2,
                             ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_batch_free(const struct ailo_llama_batch *argument_0,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_batch_get_one(ailo_i32 *argument_0, ailo_i32 argument_1,
                                     struct ailo_llama_batch **output,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_batch_init(ailo_i32 argument_0, ailo_i32 argument_1,
                                  ailo_i32 argument_2,
                                  struct ailo_llama_batch **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_chat_apply_template(
    const char *argument_0, const struct ailo_llama_chat_message *argument_1,
    size_t argument_2, ailo_bool argument_3, char *argument_4,
    ailo_i32 argument_5, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_chat_builtin_templates(const char **argument_0,
                                              size_t argument_1,
                                              ailo_i32 *output,
                                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_context_default_params(struct ailo_llama_context_params **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_copy_state_data(struct ailo_llama_context *argument_0,
                                       ailo_u8 *argument_1, size_t *output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_decode(struct ailo_llama_context *argument_0,
                              const struct ailo_llama_batch *argument_1,
                              ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_detach_threadpool(struct ailo_llama_context *argument_0,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_detokenize(const struct ailo_llama_vocab *argument_0,
                                  const ailo_i32 *argument_1,
                                  ailo_i32 argument_2, char *argument_3,
                                  ailo_i32 argument_4, ailo_bool argument_5,
                                  ailo_bool argument_6, ailo_i32 *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_encode(struct ailo_llama_context *argument_0,
                              const struct ailo_llama_batch *argument_1,
                              ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_flash_attn_type_name(enum ailo_llama_flash_attn_type argument_0,
                                const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_free(struct ailo_llama_context *argument_0,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_free_model(struct ailo_llama_model *argument_0,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_ftype_name(enum ailo_llama_ftype argument_0,
                                  const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_get_causal_attn(const struct ailo_llama_context *argument_0,
                           ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_get_embeddings(struct ailo_llama_context *argument_0,
                                      float **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_get_embeddings_ith(struct ailo_llama_context *argument_0,
                                          ailo_i32 argument_1, float **output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_get_embeddings_seq(struct ailo_llama_context *argument_0,
                                          ailo_i32 argument_1, float **output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_get_logits(struct ailo_llama_context *argument_0,
                                  float **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_get_logits_ith(struct ailo_llama_context *argument_0,
                                      ailo_i32 argument_1, float **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_get_memory(const struct ailo_llama_context *argument_0,
                                  struct ailo_llama_memory_i **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_get_model(const struct ailo_llama_context *argument_0,
                                 const struct ailo_llama_model **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_get_sampled_candidates_count_ith(
    struct ailo_llama_context *argument_0, ailo_i32 argument_1,
    ailo_u32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_get_sampled_candidates_ith(struct ailo_llama_context *argument_0,
                                      ailo_i32 argument_1, ailo_i32 **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_get_sampled_logits_count_ith(struct ailo_llama_context *argument_0,
                                        ailo_i32 argument_1, ailo_u32 *output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_get_sampled_logits_ith(struct ailo_llama_context *argument_0,
                                  ailo_i32 argument_1, float **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_get_sampled_probs_count_ith(struct ailo_llama_context *argument_0,
                                       ailo_i32 argument_1, ailo_u32 *output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_get_sampled_probs_ith(struct ailo_llama_context *argument_0,
                                 ailo_i32 argument_1, float **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_get_sampled_token_ith(struct ailo_llama_context *argument_0,
                                 ailo_i32 argument_1, ailo_i32 *output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_get_state_size(struct ailo_llama_context *argument_0,
                                      size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_init_from_model(struct ailo_llama_model *argument_0,
                           const struct ailo_llama_context_params *argument_1,
                           struct ailo_llama_context **output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_load_mode_from_str(const char *argument_0,
                                          enum ailo_llama_load_mode *output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_load_mode_name(enum ailo_llama_load_mode argument_0,
                                      const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_load_model_from_file(
    const char *argument_0, const struct ailo_llama_model_params *argument_1,
    struct ailo_llama_model **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_load_session_file(struct ailo_llama_context *argument_0,
                                         const char *argument_1,
                                         ailo_i32 *argument_2,
                                         size_t argument_3, size_t *argument_4,
                                         ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_max_devices(size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_max_parallel_sequences(size_t *output,
                                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_max_tensor_buft_overrides(size_t *output,
                                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_memory_can_shift(struct ailo_llama_memory_i *argument_0,
                                        ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_memory_clear(struct ailo_llama_memory_i *argument_0,
                                    ailo_bool argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_memory_seq_add(struct ailo_llama_memory_i *argument_0,
                                      ailo_i32 argument_1, ailo_i32 argument_2,
                                      ailo_i32 argument_3, ailo_i32 argument_4,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_memory_seq_cp(struct ailo_llama_memory_i *argument_0,
                                     ailo_i32 argument_1, ailo_i32 argument_2,
                                     ailo_i32 argument_3, ailo_i32 argument_4,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_memory_seq_div(struct ailo_llama_memory_i *argument_0,
                                      ailo_i32 argument_1, ailo_i32 argument_2,
                                      ailo_i32 argument_3, int argument_4,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_memory_seq_keep(struct ailo_llama_memory_i *argument_0,
                                       ailo_i32 argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_memory_seq_pos_max(struct ailo_llama_memory_i *argument_0,
                              ailo_i32 argument_1, ailo_i32 *output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_memory_seq_pos_min(struct ailo_llama_memory_i *argument_0,
                              ailo_i32 argument_1, ailo_i32 *output,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_memory_seq_rm(struct ailo_llama_memory_i *argument_0,
                                     ailo_i32 argument_1, ailo_i32 argument_2,
                                     ailo_i32 argument_3, ailo_bool *output,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_chat_template(const struct ailo_llama_model *argument_0,
                               const char *argument_1, const char **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_cls_label(const struct ailo_llama_model *argument_0,
                           ailo_u32 argument_1, const char **output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_decoder_start_token(const struct ailo_llama_model *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_default_params(struct ailo_llama_model_params **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_desc(const struct ailo_llama_model *argument_0,
                                  char *argument_1, size_t argument_2,
                                  ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_free(struct ailo_llama_model *argument_0,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_ftype(const struct ailo_llama_model *argument_0,
                                   enum ailo_llama_ftype *output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_get_vocab(const struct ailo_llama_model *argument_0,
                           const struct ailo_llama_vocab **output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_has_decoder(const struct ailo_llama_model *argument_0,
                             ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_has_encoder(const struct ailo_llama_model *argument_0,
                             ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_is_diffusion(const struct ailo_llama_model *argument_0,
                              ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_is_hybrid(const struct ailo_llama_model *argument_0,
                           ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_is_recurrent(const struct ailo_llama_model *argument_0,
                              ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_load_from_file(
    const char *argument_0, const struct ailo_llama_model_params *argument_1,
    struct ailo_llama_model **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_load_from_file_ptr(
    FILE *argument_0, const struct ailo_llama_model_params *argument_1,
    struct ailo_llama_model **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_load_from_splits(
    const char **argument_0, size_t argument_1,
    const struct ailo_llama_model_params *argument_2,
    struct ailo_llama_model **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_meta_count(const struct ailo_llama_model *argument_0,
                            ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_meta_key_by_index(
    const struct ailo_llama_model *argument_0, ailo_i32 argument_1,
    char *argument_2, size_t argument_3, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_meta_key_str(enum ailo_llama_model_meta_key argument_0,
                              const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_meta_val_str(
    const struct ailo_llama_model *argument_0, const char *argument_1,
    char *argument_2, size_t argument_3, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_meta_val_str_by_index(
    const struct ailo_llama_model *argument_0, ailo_i32 argument_1,
    char *argument_2, size_t argument_3, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_n_cls_out(const struct ailo_llama_model *argument_0,
                           ailo_u32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_n_ctx_train(const struct ailo_llama_model *argument_0,
                             ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_n_embd(const struct ailo_llama_model *argument_0,
                                    ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_n_embd_inp(const struct ailo_llama_model *argument_0,
                            ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_n_embd_out(const struct ailo_llama_model *argument_0,
                            ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_n_head(const struct ailo_llama_model *argument_0,
                                    ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_n_head_kv(const struct ailo_llama_model *argument_0,
                           ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_n_layer(const struct ailo_llama_model *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_n_layer_nextn(const struct ailo_llama_model *argument_0,
                               ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_n_params(const struct ailo_llama_model *argument_0,
                                      ailo_u64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_n_swa(const struct ailo_llama_model *argument_0,
                                   ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_quantize(
    const char *argument_0, const char *argument_1,
    const struct ailo_llama_model_quantize_params *argument_2, ailo_u32 *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_quantize_default_params(
    struct ailo_llama_model_quantize_params **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_rope_freq_scale_train(
    const struct ailo_llama_model *argument_0, float *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_rope_type(const struct ailo_llama_model *argument_0,
                           enum ailo_llama_rope_type *output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_model_save_to_file(const struct ailo_llama_model *argument_0,
                              const char *argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_model_size(const struct ailo_llama_model *argument_0,
                                  ailo_u64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_batch(const struct ailo_llama_context *argument_0,
                               ailo_u32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_ctx(const struct ailo_llama_context *argument_0,
                             ailo_u32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_ctx_seq(const struct ailo_llama_context *argument_0,
                                 ailo_u32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_ctx_train(const struct ailo_llama_model *argument_0,
                                   ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_embd(const struct ailo_llama_model *argument_0,
                              ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_head(const struct ailo_llama_model *argument_0,
                              ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_layer(const struct ailo_llama_model *argument_0,
                               ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_rs_seq(const struct ailo_llama_context *argument_0,
                                ailo_u32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_seq_max(const struct ailo_llama_context *argument_0,
                                 ailo_u32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_threads(struct ailo_llama_context *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_threads_batch(struct ailo_llama_context *argument_0,
                                       ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_ubatch(const struct ailo_llama_context *argument_0,
                                ailo_u32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_n_vocab(const struct ailo_llama_vocab *argument_0,
                               ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_new_context_with_model(
    struct ailo_llama_model *argument_0,
    const struct ailo_llama_context_params *argument_1,
    struct ailo_llama_context **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_numa_init(enum ailo_ggml_numa_strategy argument_0,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_opt_init(struct ailo_llama_context *argument_0,
                                struct ailo_llama_model *argument_1,
                                const struct ailo_llama_opt_params *argument_2,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_opt_param_filter_all(const struct ailo_ggml_tensor *argument_0,
                                void *argument_1, ailo_bool *output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_perf_context(const struct ailo_llama_context *argument_0,
                        struct ailo_llama_perf_context_data **output,
                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_perf_context_print(const struct ailo_llama_context *argument_0,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_perf_context_reset(struct ailo_llama_context *argument_0,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_perf_sampler(const struct ailo_llama_sampler *argument_0,
                        struct ailo_llama_perf_sampler_data **output,
                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_perf_sampler_print(const struct ailo_llama_sampler *argument_0,
                              ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_perf_sampler_reset(struct ailo_llama_sampler *argument_0,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_pooling_type(const struct ailo_llama_context *argument_0,
                                    enum ailo_llama_pooling_type *output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_print_system_info(const char **output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_process(struct ailo_llama_context *argument_0,
                               enum ailo_llama_process_type argument_1,
                               struct ailo_llama_batch_ext *argument_2,
                               ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_accept(struct ailo_llama_sampler *argument_0,
                                      ailo_i32 argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_sampler_apply(struct ailo_llama_sampler *argument_0,
                         struct ailo_llama_token_data_array *argument_1,
                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_chain_add(struct ailo_llama_sampler *argument_0,
                                         struct ailo_llama_sampler *argument_1,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_chain_default_params(
    struct ailo_llama_sampler_chain_params **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_chain_get(struct ailo_llama_sampler *argument_0,
                                         ailo_i32 argument_1,
                                         struct ailo_llama_sampler **output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_chain_init(
    const struct ailo_llama_sampler_chain_params *argument_0,
    struct ailo_llama_sampler **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_sampler_chain_n(const struct ailo_llama_sampler *argument_0,
                           ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_chain_remove(
    struct ailo_llama_sampler *argument_0, ailo_i32 argument_1,
    struct ailo_llama_sampler **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_sampler_clone(const struct ailo_llama_sampler *argument_0,
                         struct ailo_llama_sampler **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_copy(const struct ailo_llama_sampler *argument_0,
                                    struct ailo_llama_sampler *argument_1,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_free(struct ailo_llama_sampler *argument_0,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_sampler_get_seed(const struct ailo_llama_sampler *argument_0,
                            ailo_u32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init(struct ailo_llama_sampler_i *argument_0,
                                    void *argument_1,
                                    struct ailo_llama_sampler **output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_adaptive_p(
    float argument_0, float argument_1, ailo_u32 argument_2,
    struct ailo_llama_sampler **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_dist(ailo_u32 argument_0,
                                         struct ailo_llama_sampler **output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_dry(
    const struct ailo_llama_vocab *argument_0, float argument_1,
    float argument_2, ailo_i32 argument_3, ailo_i32 argument_4,
    const char **argument_5, size_t argument_6,
    struct ailo_llama_sampler **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_sampler_init_grammar(const struct ailo_llama_vocab *argument_0,
                                const char *argument_1, const char *argument_2,
                                struct ailo_llama_sampler **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_grammar_lazy(
    const struct ailo_llama_vocab *argument_0, const char *argument_1,
    const char *argument_2, const char **argument_3, size_t argument_4,
    const ailo_i32 *argument_5, size_t argument_6,
    struct ailo_llama_sampler **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_grammar_lazy_patterns(
    const struct ailo_llama_vocab *argument_0, const char *argument_1,
    const char *argument_2, const char **argument_3, size_t argument_4,
    const ailo_i32 *argument_5, size_t argument_6,
    struct ailo_llama_sampler **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_greedy(struct ailo_llama_sampler **output,
                                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_sampler_init_infill(const struct ailo_llama_vocab *argument_0,
                               struct ailo_llama_sampler **output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_logit_bias(
    ailo_i32 argument_0, ailo_i32 argument_1,
    const struct ailo_llama_logit_bias *argument_2,
    struct ailo_llama_sampler **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_min_p(float argument_0, size_t argument_1,
                                          struct ailo_llama_sampler **output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_mirostat(ailo_i32 argument_0,
                                             ailo_u32 argument_1,
                                             float argument_2, float argument_3,
                                             ailo_i32 argument_4,
                                             struct ailo_llama_sampler **output,
                                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_mirostat_v2(
    ailo_u32 argument_0, float argument_1, float argument_2,
    struct ailo_llama_sampler **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_penalties(
    ailo_i32 argument_0, ailo_i32 argument_1, float argument_2,
    float argument_3, float argument_4, struct ailo_llama_sampler **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_temp(float argument_0,
                                         struct ailo_llama_sampler **output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_temp_ext(float argument_0, float argument_1,
                                             float argument_2,
                                             struct ailo_llama_sampler **output,
                                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_top_k(ailo_i32 argument_0,
                                          struct ailo_llama_sampler **output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_top_n_sigma(
    float argument_0, struct ailo_llama_sampler **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_top_p(float argument_0, size_t argument_1,
                                          struct ailo_llama_sampler **output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_typical(float argument_0, size_t argument_1,
                                            struct ailo_llama_sampler **output,
                                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_init_xtc(float argument_0, float argument_1,
                                        size_t argument_2, ailo_u32 argument_3,
                                        struct ailo_llama_sampler **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_name(const struct ailo_llama_sampler *argument_0,
                                    const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_reset(struct ailo_llama_sampler *argument_0,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_sampler_sample(struct ailo_llama_sampler *argument_0,
                                      struct ailo_llama_context *argument_1,
                                      ailo_i32 argument_2, ailo_i32 *output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_save_session_file(struct ailo_llama_context *argument_0,
                                         const char *argument_1,
                                         const ailo_i32 *argument_2,
                                         size_t argument_3, ailo_bool *output,
                                         ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_set_adapter_cvec(struct ailo_llama_context *argument_0,
                                        const float *argument_1,
                                        size_t argument_2, ailo_i32 argument_3,
                                        ailo_i32 argument_4,
                                        ailo_i32 argument_5, ailo_i32 *output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_set_adapters_lora(
    struct ailo_llama_context *argument_0,
    struct ailo_storage_struct_llama_adapter_lora *argument_1,
    size_t argument_2, float *argument_3, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_set_causal_attn(struct ailo_llama_context *argument_0,
                                       ailo_bool argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_set_embeddings(struct ailo_llama_context *argument_0,
                                      ailo_bool argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_set_n_threads(struct ailo_llama_context *argument_0,
                                     ailo_i32 argument_1, ailo_i32 argument_2,
                                     ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_set_sampler(struct ailo_llama_context *argument_0,
                                   ailo_i32 argument_1,
                                   struct ailo_llama_sampler *argument_2,
                                   ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_set_state_data(struct ailo_llama_context *argument_0,
                                      const ailo_u8 *argument_1, size_t *output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_set_warmup(struct ailo_llama_context *argument_0,
                                  ailo_bool argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_split_path(char *argument_0, size_t argument_1,
                                  const char *argument_2, ailo_i32 argument_3,
                                  ailo_i32 argument_4, ailo_i32 *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_split_prefix(char *argument_0, size_t argument_1,
                                    const char *argument_2, ailo_i32 argument_3,
                                    ailo_i32 argument_4, ailo_i32 *output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_state_get_data(struct ailo_llama_context *argument_0,
                                      ailo_u8 *argument_1, size_t argument_2,
                                      size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_state_get_size(struct ailo_llama_context *argument_0,
                                      size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_state_load_file(struct ailo_llama_context *argument_0,
                                       const char *argument_1,
                                       ailo_i32 *argument_2, size_t argument_3,
                                       size_t *argument_4, ailo_bool *output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_state_save_file(struct ailo_llama_context *argument_0,
                                       const char *argument_1,
                                       const ailo_i32 *argument_2,
                                       size_t argument_3, ailo_bool *output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_state_seq_get_data(struct ailo_llama_context *argument_0,
                                          ailo_u8 *argument_1,
                                          size_t argument_2,
                                          ailo_i32 argument_3, size_t *output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_state_seq_get_data_ext(struct ailo_llama_context *argument_0,
                                  ailo_u8 *argument_1, size_t argument_2,
                                  ailo_i32 argument_3, ailo_u32 argument_4,
                                  size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_state_seq_get_size(struct ailo_llama_context *argument_0,
                                          ailo_i32 argument_1, size_t *output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_state_seq_get_size_ext(struct ailo_llama_context *argument_0,
                                  ailo_i32 argument_1, ailo_u32 argument_2,
                                  size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_state_seq_load_file(
    struct ailo_llama_context *argument_0, const char *argument_1,
    ailo_i32 argument_2, ailo_i32 *argument_3, size_t argument_4,
    size_t *argument_5, size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_state_seq_save_file(struct ailo_llama_context *argument_0,
                               const char *argument_1, ailo_i32 argument_2,
                               const ailo_i32 *argument_3, size_t argument_4,
                               size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_state_seq_set_data(struct ailo_llama_context *argument_0,
                                          const ailo_u8 *argument_1,
                                          size_t argument_2,
                                          ailo_i32 argument_3, size_t *output,
                                          ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_state_seq_set_data_ext(struct ailo_llama_context *argument_0,
                                  const ailo_u8 *argument_1, size_t argument_2,
                                  ailo_i32 argument_3, ailo_u32 argument_4,
                                  size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_state_set_data(struct ailo_llama_context *argument_0,
                                      const ailo_u8 *argument_1,
                                      size_t argument_2, size_t *output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_supports_gpu_offload(ailo_bool *output,
                                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_supports_mlock(ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_supports_mmap(ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_supports_rpc(ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_synchronize(struct ailo_llama_context *argument_0,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_time_us(ailo_i64 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_bos(const struct ailo_llama_vocab *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_cls(const struct ailo_llama_vocab *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_eos(const struct ailo_llama_vocab *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_eot(const struct ailo_llama_vocab *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_fim_mid(const struct ailo_llama_vocab *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_fim_pad(const struct ailo_llama_vocab *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_fim_pre(const struct ailo_llama_vocab *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_fim_rep(const struct ailo_llama_vocab *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_fim_sep(const struct ailo_llama_vocab *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_fim_suf(const struct ailo_llama_vocab *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_get_attr(const struct ailo_llama_vocab *argument_0,
                                      ailo_i32 argument_1,
                                      enum ailo_llama_token_attr *output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_token_get_score(const struct ailo_llama_vocab *argument_0,
                           ailo_i32 argument_1, float *output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_get_text(const struct ailo_llama_vocab *argument_0,
                                      ailo_i32 argument_1, const char **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_token_is_control(const struct ailo_llama_vocab *argument_0,
                            ailo_i32 argument_1, ailo_bool *output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_is_eog(const struct ailo_llama_vocab *argument_0,
                                    ailo_i32 argument_1, ailo_bool *output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_nl(const struct ailo_llama_vocab *argument_0,
                                ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_pad(const struct ailo_llama_vocab *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_sep(const struct ailo_llama_vocab *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_token_to_piece(const struct ailo_llama_vocab *argument_0,
                                      ailo_i32 argument_1, char *argument_2,
                                      ailo_i32 argument_3, ailo_i32 argument_4,
                                      ailo_bool argument_5, ailo_i32 *output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_tokenize(const struct ailo_llama_vocab *argument_0,
                                const char *argument_1, ailo_i32 argument_2,
                                ailo_i32 *argument_3, ailo_i32 argument_4,
                                ailo_bool argument_5, ailo_bool argument_6,
                                ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_version(const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_bos(const struct ailo_llama_vocab *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_cls(const struct ailo_llama_vocab *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_eos(const struct ailo_llama_vocab *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_eot(const struct ailo_llama_vocab *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_fim_mid(const struct ailo_llama_vocab *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_fim_pad(const struct ailo_llama_vocab *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_fim_pre(const struct ailo_llama_vocab *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_fim_rep(const struct ailo_llama_vocab *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_fim_sep(const struct ailo_llama_vocab *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_fim_suf(const struct ailo_llama_vocab *argument_0,
                                     ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_vocab_get_add_bos(const struct ailo_llama_vocab *argument_0,
                             ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_vocab_get_add_eos(const struct ailo_llama_vocab *argument_0,
                             ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_vocab_get_add_sep(const struct ailo_llama_vocab *argument_0,
                             ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_get_attr(const struct ailo_llama_vocab *argument_0,
                                      ailo_i32 argument_1,
                                      enum ailo_llama_token_attr *output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_vocab_get_score(const struct ailo_llama_vocab *argument_0,
                           ailo_i32 argument_1, float *output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_get_suppress_tokens(
    const struct ailo_llama_vocab *argument_0, ailo_i32 *argument_1,
    const ailo_i32 **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_get_text(const struct ailo_llama_vocab *argument_0,
                                      ailo_i32 argument_1, const char **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_llama_vocab_is_control(const struct ailo_llama_vocab *argument_0,
                            ailo_i32 argument_1, ailo_bool *output,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_is_eog(const struct ailo_llama_vocab *argument_0,
                                    ailo_i32 argument_1, ailo_bool *output,
                                    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_mask(const struct ailo_llama_vocab *argument_0,
                                  ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_n_tokens(const struct ailo_llama_vocab *argument_0,
                                      ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_nl(const struct ailo_llama_vocab *argument_0,
                                ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_pad(const struct ailo_llama_vocab *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_sep(const struct ailo_llama_vocab *argument_0,
                                 ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_llama_vocab_type(const struct ailo_llama_vocab *argument_0,
                                  enum ailo_llama_vocab_type *output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_batch_add_chunk(struct ailo_mtmd_batch *argument_0,
                          const struct ailo_mtmd_input_chunk *argument_1,
                          ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_batch_encode(struct ailo_mtmd_batch *argument_0,
                                   ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_batch_free(struct ailo_mtmd_batch *argument_0,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_batch_get_output_embd(struct ailo_mtmd_batch *argument_0,
                                const struct ailo_mtmd_input_chunk *argument_1,
                                float **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_batch_init(struct ailo_mtmd_context *argument_0,
                                 struct ailo_mtmd_batch **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_bitmap_free(struct ailo_mtmd_bitmap *argument_0,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_bitmap_get_data(const struct ailo_mtmd_bitmap *argument_0,
                                      const unsigned char **output,
                                      ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_bitmap_get_id(const struct ailo_mtmd_bitmap *argument_0,
                                    const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_bitmap_get_n_bytes(const struct ailo_mtmd_bitmap *argument_0,
                             size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_bitmap_get_nx(const struct ailo_mtmd_bitmap *argument_0,
                                    ailo_u32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_bitmap_get_ny(const struct ailo_mtmd_bitmap *argument_0,
                                    ailo_u32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_bitmap_init(ailo_u32 argument_0, ailo_u32 argument_1,
                                  const unsigned char *argument_2,
                                  struct ailo_mtmd_bitmap **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_bitmap_init_from_audio(size_t argument_0,
                                             const float *argument_1,
                                             struct ailo_mtmd_bitmap **output,
                                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_bitmap_is_audio(const struct ailo_mtmd_bitmap *argument_0,
                                      ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_bitmap_set_id(struct ailo_mtmd_bitmap *argument_0,
                                    const char *argument_1, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_bitmap_set_mergeable(struct ailo_mtmd_bitmap *argument_0,
                                           ailo_bool argument_1,
                                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_context_params_default(struct ailo_mtmd_context_params **output,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_decode_use_mrope(const struct ailo_mtmd_context *argument_0,
                           ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_decode_use_non_causal(const struct ailo_mtmd_context *argument_0,
                                const struct ailo_mtmd_input_chunk *argument_1,
                                ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_default_marker(const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_encode(struct ailo_mtmd_context *argument_0,
                             const struct ailo_mtmd_image_tokens *argument_1,
                             ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_encode_chunk(struct ailo_mtmd_context *argument_0,
                       const struct ailo_mtmd_input_chunk *argument_1,
                       ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_free(struct ailo_mtmd_context *argument_0,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_gen_audio_get_info(const struct ailo_mtmd_context *argument_0,
                             struct ailo_mtmd_gen_audio_info **output,
                             ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_gen_audio_process(struct ailo_mtmd_context *argument_0,
                            const struct ailo_mtmd_gen_inp *argument_1,
                            struct ailo_mtmd_gen_out *argument_2,
                            ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_gen_inp_default(const struct ailo_mtmd_context *argument_0,
                          struct ailo_mtmd_gen_inp **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_get_audio_sample_rate(const struct ailo_mtmd_context *argument_0,
                                int *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_get_cap_from_file(const char *argument_0,
                                        struct ailo_mtmd_caps **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_get_marker(const struct ailo_mtmd_context *argument_0,
                                 const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_get_memory_usage(
    const char *argument_0, const struct ailo_mtmd_context_params *argument_1,
    struct ailo_mtmd_memory_usage **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_get_output_embd(struct ailo_mtmd_context *argument_0,
                                      float **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_bitmap_init_from_buf(
    const struct ailo_mtmd_context *argument_0, const unsigned char *argument_1,
    size_t argument_2, ailo_bool argument_3,
    const struct ailo_mtmd_helper_init_opt *argument_4,
    struct ailo_mtmd_helper_bitmap_wrapper **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_bitmap_init_from_file(
    const struct ailo_mtmd_context *argument_0, const char *argument_1,
    ailo_bool argument_2, const struct ailo_mtmd_helper_init_opt *argument_3,
    struct ailo_mtmd_helper_bitmap_wrapper **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_eval_chunk_single(
    struct ailo_mtmd_context *argument_0, struct ailo_llama_context *argument_1,
    const struct ailo_mtmd_input_chunk *argument_2, ailo_i32 argument_3,
    ailo_i32 argument_4, ailo_i32 argument_5, ailo_bool argument_6,
    ailo_i32 *argument_7, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_eval_chunks(
    struct ailo_mtmd_context *argument_0, struct ailo_llama_context *argument_1,
    const struct ailo_mtmd_input_chunks *argument_2, ailo_i32 argument_3,
    ailo_i32 argument_4, ailo_i32 argument_5, ailo_bool argument_6,
    ailo_i32 *argument_7, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_helper_gen_audio_free(struct ailo_mtmd_helper_gen_audio *argument_0,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_gen_audio_get_output(
    struct ailo_mtmd_helper_gen_audio *argument_0, ailo_i32 *argument_1,
    const char **argument_2, size_t *argument_3,
    struct ailo_storage_int64_t *argument_4, ailo_i32 *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_gen_audio_init(
    struct ailo_llama_context *argument_0, struct ailo_mtmd_context *argument_1,
    struct ailo_mtmd_helper_gen_audio **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_helper_gen_audio_reset(struct ailo_mtmd_helper_gen_audio *argument_0,
                                 ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_gen_audio_set_input(
    struct ailo_mtmd_helper_gen_audio *argument_0,
    const struct ailo_mtmd_helper_gen_audio_inp *argument_1, ailo_i32 *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_gen_audio_step_gen(
    struct ailo_mtmd_helper_gen_audio *argument_0, ailo_i32 argument_1,
    const float *argument_2, const float **argument_3,
    struct ailo_storage_bool *argument_4, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_gen_audio_step_prompt(
    struct ailo_mtmd_helper_gen_audio *argument_0, ailo_i32 argument_1,
    ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_helper_get_n_pos(const struct ailo_mtmd_input_chunks *argument_0,
                           ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_helper_get_n_tokens(const struct ailo_mtmd_input_chunks *argument_0,
                              size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_image_get_decoder_pos(
    const struct ailo_mtmd_image_tokens *argument_0, ailo_i32 argument_1,
    struct ailo_mtmd_decoder_pos *argument_2, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_helper_init_opt_default(struct ailo_mtmd_helper_init_opt **output,
                                  ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_helper_model_can_chat(const struct ailo_llama_context *argument_0,
                                const struct ailo_mtmd_context *argument_1,
                                ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_helper_support_video(const struct ailo_mtmd_context *argument_0,
                               ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_helper_video_free(struct ailo_mtmd_helper_video *argument_0,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_helper_video_get_info(const struct ailo_mtmd_helper_video *argument_0,
                                struct ailo_mtmd_helper_video_info **output,
                                ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_video_init(
    const struct ailo_mtmd_context *argument_0, const char *argument_1,
    const struct ailo_mtmd_helper_video_init_params *argument_2,
    struct ailo_mtmd_helper_video **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_video_init_from_buf(
    const struct ailo_mtmd_context *argument_0, const unsigned char *argument_1,
    size_t argument_2,
    const struct ailo_mtmd_helper_video_init_params *argument_3,
    struct ailo_mtmd_helper_video **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_video_init_params_default(
    struct ailo_mtmd_helper_video_init_params **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_helper_video_read_next(
    struct ailo_mtmd_helper_video *argument_0,
    struct ailo_storage_struct_mtmd_bitmap *argument_1, char **argument_2,
    ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_image_tokens_get_decoder_pos(
    const struct ailo_mtmd_image_tokens *argument_0, ailo_i32 argument_1,
    size_t argument_2, struct ailo_mtmd_decoder_pos **output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_image_tokens_get_id(const struct ailo_mtmd_image_tokens *argument_0,
                              const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_image_tokens_get_n_pos(
    const struct ailo_mtmd_image_tokens *argument_0, ailo_i32 *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_image_tokens_get_n_tokens(
    const struct ailo_mtmd_image_tokens *argument_0, size_t *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_image_tokens_get_nx(const struct ailo_mtmd_image_tokens *argument_0,
                              size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_image_tokens_get_ny(const struct ailo_mtmd_image_tokens *argument_0,
                              size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_init_from_file(const char *argument_0,
                         const struct ailo_llama_model *argument_1,
                         const struct ailo_mtmd_context_params *argument_2,
                         struct ailo_mtmd_context **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_input_chunk_copy(const struct ailo_mtmd_input_chunk *argument_0,
                           struct ailo_mtmd_input_chunk **output,
                           ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_input_chunk_free(struct ailo_mtmd_input_chunk *argument_0,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_input_chunk_get_id(const struct ailo_mtmd_input_chunk *argument_0,
                             const char **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_input_chunk_get_n_pos(const struct ailo_mtmd_input_chunk *argument_0,
                                ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_input_chunk_get_n_tokens(
    const struct ailo_mtmd_input_chunk *argument_0, size_t *output,
    ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_input_chunk_get_placeholder(
    const struct ailo_mtmd_input_chunk *argument_0,
    struct ailo_mtmd_input_chunk **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_input_chunk_get_tokens_image(
    const struct ailo_mtmd_input_chunk *argument_0,
    const struct ailo_mtmd_image_tokens **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_input_chunk_get_tokens_text(
    const struct ailo_mtmd_input_chunk *argument_0, size_t *argument_1,
    const ailo_i32 **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_input_chunk_get_type(const struct ailo_mtmd_input_chunk *argument_0,
                               enum ailo_mtmd_input_chunk_type *output,
                               ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_input_chunk_load(const char *argument_0,
                                       size_t argument_1,
                                       struct ailo_mtmd_input_chunk **output,
                                       ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_input_chunk_save(
    const struct ailo_mtmd_input_chunk *argument_0, char *argument_1,
    size_t argument_2, size_t *argument_3, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_input_chunks_free(struct ailo_mtmd_input_chunks *argument_0,
                            ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_input_chunks_get(
    const struct ailo_mtmd_input_chunks *argument_0, size_t argument_1,
    const struct ailo_mtmd_input_chunk **output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_input_chunks_init(struct ailo_mtmd_input_chunks **output,
                                        ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_input_chunks_size(const struct ailo_mtmd_input_chunks *argument_0,
                            size_t *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_support_audio(const struct ailo_mtmd_context *argument_0,
                                    ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_support_vision(const struct ailo_mtmd_context *argument_0,
                                     ailo_bool *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status
ailo_mtmd_test_create_input_chunks(struct ailo_mtmd_input_chunks **output,
                                   ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_tokenize(
    const struct ailo_mtmd_context *argument_0,
    struct ailo_mtmd_input_chunks *argument_1,
    const struct ailo_mtmd_input_text *argument_2,
    const struct ailo_storage_const_struct_mtmd_bitmap *argument_3,
    size_t argument_4, ailo_i32 *output, ailo_error *error);
/** Invoke the native API with its preconditions; native results and ownership
 * are preserved separately from bridge status. */
ailo_status ailo_mtmd_tokenize_from_parts(
    const struct ailo_mtmd_context *argument_0,
    struct ailo_mtmd_input_chunks *argument_1,
    const struct ailo_storage_const_struct_mtmd_input_part *argument_2,
    size_t argument_3, ailo_bool argument_4, ailo_i32 *output,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_storage_int64_t_array_new(size_t count,
                                           struct ailo_storage_int64_t **output,
                                           ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_storage_int64_t_array_free(struct ailo_storage_int64_t *self,
                                            ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_storage_int64_t_array_at(struct ailo_storage_int64_t *self,
                                          size_t count, size_t index,
                                          struct ailo_storage_int64_t **output,
                                          ailo_error *error);
/** Read a native-backed element; actual extent and owner lifetime are caller
 * obligations. */
ailo_status ailo_storage_int64_t_get(const struct ailo_storage_int64_t *self,
                                     size_t count, size_t index,
                                     ailo_i64 *output, ailo_error *error);
/** Write an explicitly converted native element within the caller-declared
 * extent. */
ailo_status ailo_storage_int64_t_set(struct ailo_storage_int64_t *self,
                                     size_t count, size_t index, ailo_i64 value,
                                     ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_storage_uint64_t_array_new(
    size_t count, struct ailo_storage_uint64_t **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_storage_uint64_t_array_free(struct ailo_storage_uint64_t *self,
                                             ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_storage_uint64_t_array_at(
    struct ailo_storage_uint64_t *self, size_t count, size_t index,
    struct ailo_storage_uint64_t **output, ailo_error *error);
/** Read a native-backed element; actual extent and owner lifetime are caller
 * obligations. */
ailo_status ailo_storage_uint64_t_get(const struct ailo_storage_uint64_t *self,
                                      size_t count, size_t index,
                                      ailo_u64 *output, ailo_error *error);
/** Write an explicitly converted native element within the caller-declared
 * extent. */
ailo_status ailo_storage_uint64_t_set(struct ailo_storage_uint64_t *self,
                                      size_t count, size_t index,
                                      ailo_u64 value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_storage_bool_array_new(size_t count,
                                        struct ailo_storage_bool **output,
                                        ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_storage_bool_array_free(struct ailo_storage_bool *self,
                                         ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_storage_bool_array_at(struct ailo_storage_bool *self,
                                       size_t count, size_t index,
                                       struct ailo_storage_bool **output,
                                       ailo_error *error);
/** Read a native-backed element; actual extent and owner lifetime are caller
 * obligations. */
ailo_status ailo_storage_bool_get(const struct ailo_storage_bool *self,
                                  size_t count, size_t index, ailo_bool *output,
                                  ailo_error *error);
/** Write an explicitly converted native element within the caller-declared
 * extent. */
ailo_status ailo_storage_bool_set(struct ailo_storage_bool *self, size_t count,
                                  size_t index, ailo_bool value,
                                  ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_storage_struct_ggml_context_array_new(
    size_t count, struct ailo_storage_struct_ggml_context **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_storage_struct_ggml_context_array_free(
    struct ailo_storage_struct_ggml_context *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_storage_struct_ggml_context_array_at(
    struct ailo_storage_struct_ggml_context *self, size_t count, size_t index,
    struct ailo_storage_struct_ggml_context **output, ailo_error *error);
/** Read a native-backed element; actual extent and owner lifetime are caller
 * obligations. */
ailo_status ailo_storage_struct_ggml_context_get(
    const struct ailo_storage_struct_ggml_context *self, size_t count,
    size_t index, struct ailo_ggml_context **output, ailo_error *error);
/** Write an explicitly converted native element within the caller-declared
 * extent. */
ailo_status ailo_storage_struct_ggml_context_set(
    struct ailo_storage_struct_ggml_context *self, size_t count, size_t index,
    struct ailo_ggml_context *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_storage_struct_ggml_backend_device_array_new(
    size_t count, struct ailo_storage_struct_ggml_backend_device **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_storage_struct_ggml_backend_device_array_free(
    struct ailo_storage_struct_ggml_backend_device *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_storage_struct_ggml_backend_device_array_at(
    struct ailo_storage_struct_ggml_backend_device *self, size_t count,
    size_t index, struct ailo_storage_struct_ggml_backend_device **output,
    ailo_error *error);
/** Read a native-backed element; actual extent and owner lifetime are caller
 * obligations. */
ailo_status ailo_storage_struct_ggml_backend_device_get(
    const struct ailo_storage_struct_ggml_backend_device *self, size_t count,
    size_t index, struct ailo_ggml_backend_device **output, ailo_error *error);
/** Write an explicitly converted native element within the caller-declared
 * extent. */
ailo_status ailo_storage_struct_ggml_backend_device_set(
    struct ailo_storage_struct_ggml_backend_device *self, size_t count,
    size_t index, struct ailo_ggml_backend_device *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_storage_struct_ggml_tensor_array_new(
    size_t count, struct ailo_storage_struct_ggml_tensor **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_storage_struct_ggml_tensor_array_free(
    struct ailo_storage_struct_ggml_tensor *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_storage_struct_ggml_tensor_array_at(
    struct ailo_storage_struct_ggml_tensor *self, size_t count, size_t index,
    struct ailo_storage_struct_ggml_tensor **output, ailo_error *error);
/** Read a native-backed element; actual extent and owner lifetime are caller
 * obligations. */
ailo_status ailo_storage_struct_ggml_tensor_get(
    const struct ailo_storage_struct_ggml_tensor *self, size_t count,
    size_t index, struct ailo_ggml_tensor **output, ailo_error *error);
/** Write an explicitly converted native element within the caller-declared
 * extent. */
ailo_status ailo_storage_struct_ggml_tensor_set(
    struct ailo_storage_struct_ggml_tensor *self, size_t count, size_t index,
    struct ailo_ggml_tensor *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_storage_struct_ggml_backend_array_new(
    size_t count, struct ailo_storage_struct_ggml_backend **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_storage_struct_ggml_backend_array_free(
    struct ailo_storage_struct_ggml_backend *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_storage_struct_ggml_backend_array_at(
    struct ailo_storage_struct_ggml_backend *self, size_t count, size_t index,
    struct ailo_storage_struct_ggml_backend **output, ailo_error *error);
/** Read a native-backed element; actual extent and owner lifetime are caller
 * obligations. */
ailo_status ailo_storage_struct_ggml_backend_get(
    const struct ailo_storage_struct_ggml_backend *self, size_t count,
    size_t index, struct ailo_ggml_backend **output, ailo_error *error);
/** Write an explicitly converted native element within the caller-declared
 * extent. */
ailo_status ailo_storage_struct_ggml_backend_set(
    struct ailo_storage_struct_ggml_backend *self, size_t count, size_t index,
    struct ailo_ggml_backend *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_storage_struct_ggml_backend_buffer_type_array_new(
    size_t count, struct ailo_storage_struct_ggml_backend_buffer_type **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_storage_struct_ggml_backend_buffer_type_array_free(
    struct ailo_storage_struct_ggml_backend_buffer_type *self,
    ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_storage_struct_ggml_backend_buffer_type_array_at(
    struct ailo_storage_struct_ggml_backend_buffer_type *self, size_t count,
    size_t index, struct ailo_storage_struct_ggml_backend_buffer_type **output,
    ailo_error *error);
/** Read a native-backed element; actual extent and owner lifetime are caller
 * obligations. */
ailo_status ailo_storage_struct_ggml_backend_buffer_type_get(
    const struct ailo_storage_struct_ggml_backend_buffer_type *self,
    size_t count, size_t index, struct ailo_ggml_backend_buffer_type **output,
    ailo_error *error);
/** Write an explicitly converted native element within the caller-declared
 * extent. */
ailo_status ailo_storage_struct_ggml_backend_buffer_type_set(
    struct ailo_storage_struct_ggml_backend_buffer_type *self, size_t count,
    size_t index, struct ailo_ggml_backend_buffer_type *value,
    ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_storage_struct_llama_adapter_lora_array_new(
    size_t count, struct ailo_storage_struct_llama_adapter_lora **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_storage_struct_llama_adapter_lora_array_free(
    struct ailo_storage_struct_llama_adapter_lora *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_storage_struct_llama_adapter_lora_array_at(
    struct ailo_storage_struct_llama_adapter_lora *self, size_t count,
    size_t index, struct ailo_storage_struct_llama_adapter_lora **output,
    ailo_error *error);
/** Read a native-backed element; actual extent and owner lifetime are caller
 * obligations. */
ailo_status ailo_storage_struct_llama_adapter_lora_get(
    const struct ailo_storage_struct_llama_adapter_lora *self, size_t count,
    size_t index, struct ailo_llama_adapter_lora **output, ailo_error *error);
/** Write an explicitly converted native element within the caller-declared
 * extent. */
ailo_status ailo_storage_struct_llama_adapter_lora_set(
    struct ailo_storage_struct_llama_adapter_lora *self, size_t count,
    size_t index, struct ailo_llama_adapter_lora *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_storage_struct_mtmd_bitmap_array_new(
    size_t count, struct ailo_storage_struct_mtmd_bitmap **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_storage_struct_mtmd_bitmap_array_free(
    struct ailo_storage_struct_mtmd_bitmap *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_storage_struct_mtmd_bitmap_array_at(
    struct ailo_storage_struct_mtmd_bitmap *self, size_t count, size_t index,
    struct ailo_storage_struct_mtmd_bitmap **output, ailo_error *error);
/** Read a native-backed element; actual extent and owner lifetime are caller
 * obligations. */
ailo_status ailo_storage_struct_mtmd_bitmap_get(
    const struct ailo_storage_struct_mtmd_bitmap *self, size_t count,
    size_t index, struct ailo_mtmd_bitmap **output, ailo_error *error);
/** Write an explicitly converted native element within the caller-declared
 * extent. */
ailo_status ailo_storage_struct_mtmd_bitmap_set(
    struct ailo_storage_struct_mtmd_bitmap *self, size_t count, size_t index,
    struct ailo_mtmd_bitmap *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_storage_const_struct_mtmd_bitmap_array_new(
    size_t count, struct ailo_storage_const_struct_mtmd_bitmap **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_storage_const_struct_mtmd_bitmap_array_free(
    struct ailo_storage_const_struct_mtmd_bitmap *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_storage_const_struct_mtmd_bitmap_array_at(
    struct ailo_storage_const_struct_mtmd_bitmap *self, size_t count,
    size_t index, struct ailo_storage_const_struct_mtmd_bitmap **output,
    ailo_error *error);
/** Read a native-backed element; actual extent and owner lifetime are caller
 * obligations. */
ailo_status ailo_storage_const_struct_mtmd_bitmap_get(
    const struct ailo_storage_const_struct_mtmd_bitmap *self, size_t count,
    size_t index, const struct ailo_mtmd_bitmap **output, ailo_error *error);
/** Write an explicitly converted native element within the caller-declared
 * extent. */
ailo_status ailo_storage_const_struct_mtmd_bitmap_set(
    struct ailo_storage_const_struct_mtmd_bitmap *self, size_t count,
    size_t index, const struct ailo_mtmd_bitmap *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_storage_const_struct_mtmd_input_part_array_new(
    size_t count, struct ailo_storage_const_struct_mtmd_input_part **output,
    ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status ailo_storage_const_struct_mtmd_input_part_array_free(
    struct ailo_storage_const_struct_mtmd_input_part *self, ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_storage_const_struct_mtmd_input_part_array_at(
    struct ailo_storage_const_struct_mtmd_input_part *self, size_t count,
    size_t index, struct ailo_storage_const_struct_mtmd_input_part **output,
    ailo_error *error);
/** Read a native-backed element; actual extent and owner lifetime are caller
 * obligations. */
ailo_status ailo_storage_const_struct_mtmd_input_part_get(
    const struct ailo_storage_const_struct_mtmd_input_part *self, size_t count,
    size_t index, const struct ailo_mtmd_input_part **output,
    ailo_error *error);
/** Write an explicitly converted native element within the caller-declared
 * extent. */
ailo_status ailo_storage_const_struct_mtmd_input_part_set(
    struct ailo_storage_const_struct_mtmd_input_part *self, size_t count,
    size_t index, const struct ailo_mtmd_input_part *value, ailo_error *error);
/** Allocate zero-initialized native-backed elements; caller owns the result and
 * count. */
ailo_status ailo_backend_memory_map_array_new(
    size_t count, struct ailo_backend_memory_map **output, ailo_error *error);
/** Release only a facade element allocation or by-value result; NULL is
 * accepted. */
ailo_status
ailo_backend_memory_map_array_free(struct ailo_backend_memory_map *self,
                                   ailo_error *error);
/** Borrow an element using the actual caller-declared extent; never free the
 * element. */
ailo_status ailo_backend_memory_map_array_at(
    struct ailo_backend_memory_map *self, size_t count, size_t index,
    struct ailo_backend_memory_map **output, ailo_error *error);
/** Read the number of device memory entries. */
ailo_status
ailo_backend_memory_map_size(const struct ailo_backend_memory_map *self,
                             size_t *output, ailo_error *error);
/** Insert or replace a device memory entry; the device remains borrowed. */
ailo_status ailo_backend_memory_map_set(struct ailo_backend_memory_map *self,
                                        struct ailo_ggml_backend_device *device,
                                        size_t bytes, ailo_error *error);
/** Borrow the indexed device and read its byte count; map ordering follows
 * native device keys. */
ailo_status
ailo_backend_memory_map_get(const struct ailo_backend_memory_map *self,
                            size_t index,
                            struct ailo_ggml_backend_device **device,
                            size_t *output, ailo_error *error);
#define AILO_GGML_BACKEND_META_MAX_DEVICES 16
#define AILO_GGML_CANN_MAX_DEVICES 16
#define AILO_GGML_CUBLAS_NAME "cuBLAS"
#define AILO_GGML_CUDA_MAX_DEVICES 16
#define AILO_GGML_CUDA_NAME "CUDA"
#define AILO_GGML_DEFAULT_GRAPH_SIZE 2048
#define AILO_GGML_DEFAULT_N_THREADS 4
#define AILO_GGML_EXIT_ABORTED 1
#define AILO_GGML_EXIT_SUCCESS 0
#define AILO_GGML_FILE_MAGIC 0x67676d6c
#define AILO_GGML_FILE_VERSION 2
#define AILO_GGML_MAX_DIMS 4
#define AILO_GGML_MAX_NAME 64
#define AILO_GGML_MAX_N_THREADS 512
#define AILO_GGML_MAX_OP_PARAMS 64
#define AILO_GGML_MAX_PARAMS 2048
#define AILO_GGML_MAX_SRC 10
#define AILO_GGML_MEM_ALIGN 16
#define AILO_GGML_MROPE_SECTIONS 4
#define AILO_GGML_OPENVINO_NAME "OPENVINO"
#define AILO_GGML_QNT_VERSION 2
#define AILO_GGML_QNT_VERSION_FACTOR 1000
#define AILO_GGML_ROPE_TYPE_IMROPE 40
#define AILO_GGML_ROPE_TYPE_MROPE 8
#define AILO_GGML_ROPE_TYPE_NEOX 2
#define AILO_GGML_ROPE_TYPE_NORMAL 0
#define AILO_GGML_ROPE_TYPE_VISION 24
#define AILO_GGML_RPC_MAX_SERVERS 16
#define AILO_GGML_SYCL_MAX_DEVICES 48
#define AILO_GGML_SYCL_NAME "SYCL"
#define AILO_GGML_VK_MAX_DEVICES 16
#define AILO_GGML_VK_NAME "Vulkan"
#define AILO_GGML_WEBGPU_NAME "WebGPU"
#define AILO_GGUF_DEFAULT_ALIGNMENT 32
#define AILO_GGUF_KEY_GENERAL_ALIGNMENT "general.alignment"
#define AILO_GGUF_MAGIC "GGUF"
#define AILO_GGUF_VERSION 3
#define AILO_LLAMA_DEFAULT_SEED 0xFFFFFFFF
#define AILO_LLAMA_FILE_MAGIC_GGLA 0x67676c61u
#define AILO_LLAMA_FILE_MAGIC_GGSN 0x6767736eu
#define AILO_LLAMA_FILE_MAGIC_GGSQ 0x67677371u
#define AILO_LLAMA_SESSION_MAGIC AILO_LLAMA_FILE_MAGIC_GGSN
#define AILO_LLAMA_SESSION_VERSION 11
#define AILO_LLAMA_STATE_SEQ_FLAGS_NONE 0
#define AILO_LLAMA_STATE_SEQ_FLAGS_ON_DEVICE 2
#define AILO_LLAMA_STATE_SEQ_FLAGS_PARTIAL_ONLY 1
#define AILO_LLAMA_STATE_SEQ_FLAGS_SWA_ONLY 1
#define AILO_LLAMA_STATE_SEQ_MAGIC AILO_LLAMA_FILE_MAGIC_GGSQ
#define AILO_LLAMA_STATE_SEQ_VERSION 4
#define AILO_LLAMA_TOKEN_NULL -1
#ifdef __cplusplus
}
#endif
#endif
