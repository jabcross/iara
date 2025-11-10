#include "Iara/Passes/VirtualFIFO/SDF/BufferSizeCalculator.h"
#include "mlir/Analysis/Presburger/IntegerRelation.h"
#include "mlir/Analysis/Presburger/PresburgerSpace.h"

using namespace iara::passes::virtualfifo::sdf;
using mlir::presburger::IntegerRelation;
using mlir::presburger::PresburgerSpace;

// Solves a system of Diophantine equations to find the minimal positive
// integer solution for the execution counts (k-values) of the actors in an SDF
// chain. Minimizes the buffer size: rate[n-1] * k[n-1]
static llvm::FailureOr<llvm::SmallVector<int64_t>>
solveSystem(llvm::SmallVector<i64> &rates, llvm::SmallVector<i64> &delays,
            bool useDelays) {
  int numVars = rates.size();
  // Add one extra variable for the objective: bufferSize = rate[n-1] * k[n-1]
  // Variable order: [bufferSize, k[0], k[1], ..., k[n-1]]
  PresburgerSpace space = PresburgerSpace::getSetSpace(numVars + 1);
  IntegerRelation rel(space);

  // Add equality constraint for objective: bufferSize = rate[n-1] * k[n-1]
  // Rewritten as: bufferSize - rate[n-1] * k[n-1] = 0
  {
    llvm::SmallVector<int64_t> eq(numVars + 2, 0);
    eq[0] = 1;                      // coefficient for bufferSize (first variable)
    eq[numVars] = -rates[numVars - 1]; // coefficient for k[n-1] (last k variable)
    rel.addEquality(eq);
  }

  // Add equality constraints: rate[i] * k[i] - rate[i+1] * k[i+1] + delay[i] = 0
  // k[i] is at index i+1 (since bufferSize is at index 0)
  for (unsigned i = 0; i < delays.size(); ++i) {
    llvm::SmallVector<int64_t> eq(numVars + 2, 0);
    eq[i + 1] = rates[i];           // coefficient for k[i]
    eq[i + 2] = -rates[i + 1];      // coefficient for k[i+1]
    if (useDelays) {
      eq[numVars + 1] = delays[i];  // constant term (last position)
    }
    rel.addEquality(eq);
  }

  // Add inequality constraints: k[i] >= 1 for i > 0
  // Python version only requires alpha[i+1] >= 1, not alpha[0] >= 1
  // k[i] is at index i+1 (since bufferSize is at index 0)
  for (int i = 1; i < numVars; ++i) {
    llvm::SmallVector<int64_t> ineq(numVars + 2, 0);
    ineq[i + 1] = 1;                // coefficient for k[i]
    ineq[numVars + 1] = -1;         // constant term
    rel.addInequality(ineq);        // k_i - 1 >= 0
  }

  // Find lexicographically minimal solution
  // Since bufferSize is the first variable, this minimizes it first
  auto lexmin = rel.findIntegerLexMin();
  if (!lexmin.isBounded()) {
    return llvm::failure();
  }

  // Extract k-values (skip the first variable which is bufferSize)
  llvm::SmallVector<int64_t> result;
  auto lexminVec = *lexmin;
  for (size_t i = 1; i < lexminVec.size(); ++i) {
    result.push_back(static_cast<int64_t>(lexminVec[i]));
  }
  return result;
}

llvm::FailureOr<BufferSizeValues *>
iara::passes::virtualfifo::sdf::calculateBufferSizePresburger(
    BufferSizeMemo &memo, llvm::SmallVector<i64> &rates,
    llvm::SmallVector<i64> &delays) {

  auto [existing, values] = memo.get(rates, delays);
  if (existing) {
    return values;
  }

  assert(rates.size() == delays.size() + 1 && "Invalid rates/delays size");

  if (rates.size() == 2 && rates[0] == rates[1] && delays[0] == 0) {
    *values = BufferSizeValues{{1, 1}, {1, 1}};
    return values;
  }

  // Calculate alpha (with delays)
  auto alphaResult = solveSystem(rates, delays, /*useDelays=*/true);
  if (failed(alphaResult)) {
    return llvm::failure();
  }

  // Calculate beta (without delays)
  llvm::SmallVector<i64> noDelays(delays.size(), 0);
  auto betaResult = solveSystem(rates, noDelays, /*useDelays=*/false);
  if (failed(betaResult)) {
    return llvm::failure();
  }

  values->alpha = *alphaResult;
  values->beta = *betaResult;

  return values;
}
