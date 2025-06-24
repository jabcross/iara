#include "Iara/Dialect/IaraOps.h"
#include "Iara/SDF/Analysis.h"
#include "Iara/SDF/BufferSizeCalculator.h"
#include <llvm/Support/ErrorHandling.h>
#include <mlir/Support/LogicalResult.h>

namespace iara::sdf {
using namespace iara::dialect;

LogicalResult analyseVirtualBufferSizes(ActorOp actor,
                                        iara::sdf::StaticAnalysisData &data) {
  auto chains = getInoutChains(actor);
  for (auto &chain : chains) {

    Vec<i64> rates, delays;

    rates.push_back(data.edge_static_info[chain.front()].prod_rate);

    for (auto edge : chain) {
      auto info = &data.edge_static_info[edge];
      rates.push_back(info->cons_rate);
      delays.push_back(info->delay_size);
    }

    assert(rates.size() == chain.size() + 1);
    assert(delays.size() == chain.size());

    auto buffer_values = calculateBufferSize(rates, delays);
    if (failed(buffer_values)) {
      return failure();
    }

    auto first_buffer_size =
        buffer_values->alpha.back() * rates.back() + delays.back();
    auto next_buffer_sizes = buffer_values->beta.back() * rates.back();

    for (auto [i, edge] : enumerate(chain)) {
      auto &info = data.edge_static_info[edge];

      info.block_size_with_delays = first_buffer_size;
      info.block_size_no_delays = next_buffer_sizes;
      info.prod_alpha = buffer_values->alpha[i];
      info.prod_beta = buffer_values->beta[i];
      info.cons_alpha = buffer_values->alpha[i + 1];
      info.cons_beta = buffer_values->beta[i + 1];
    }
  }
  return success();
}

} // namespace iara::sdf
