#include "IaraRuntime/Chunk.h"
#include "IaraRuntime/SDF_OoO_Node.h"

void SDF_OoO_Node::init() {
  semaphore_map.total_resources = info.input_bytes;
  semaphore_map.first_time_func = [_this = this](None,
                                                 std::vector<Chunk> &args) {
    // Init vector with appropriate size.
    assert(args.empty());
    args = std::vector<Chunk>{(size_t)_this->num_inputs};
  };
  semaphore_map.every_time_func = [](EveryTimeArgs &et_args,
                                     std::vector<Chunk> &args) {
    auto &[new_chunk, idx, first] = et_args;
    auto &old_chunk = args[idx];
    assert(args.size() > (size_t)idx);
    assert(old_chunk.is_released());
    // only the first chunk of a firing contains the right pointer.
    if (first) {
      old_chunk = new_chunk;
    }
  };
  semaphore_map.last_time_func = [_this = this](ConsSeq &seq,
                                                std::vector<Chunk> &args) {
    std::vector<Chunk> pass{};
    pass.swap(args);
    _this->fire(seq, std::move(pass));
  };
};

void SDF_OoO_Node::consume(i64 seq, Chunk chunk, i64 arg_idx,
                           i64 offset_partial) {
  None n = {};
  EveryTimeArgs et_args{.data = std::move(chunk),
                        .arg_idx = arg_idx,
                        .first_of_firing = (offset_partial == 0)};
  semaphore_map.arrive(seq, chunk.data_size(), n, et_args, seq);
}

void SDF_OoO_Node::fire(i64 seq, std::vector<Chunk> &&args) {
  wrapper(seq, args);
}
