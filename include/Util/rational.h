#ifndef RATIONAL_H
#define RATIONAL_H

#include <compare>
#include <llvm/Support/FormatProviders.h>
#include <mlir/Support/MathExtras.h>
#include <numeric>

namespace util {

// Rational number representation. Keeps precision between successive
// multiplications/divisions, as long as both numerator and denominator can fit
// into int64_t.
//
// Addition, subtraction and comparison take O(log N) time, as the LCM of
// the denominators must be taken.
//
// Normalization consists of making the ratio irreducible and the denominator
// positive. It should be called manually before the number is stored.
struct Rational {
  int64_t num;
  int64_t denom;

  Rational() : num(0), denom(1) {}
  Rational(int64_t from) : num(from), denom(1) {}
  Rational(int64_t num, int64_t denom) : num(num), denom(denom) {}

  Rational operator+(Rational const &other) const {
    auto lcm = std::lcm(denom, other.denom);
    auto n = num * (lcm / denom) + other.num * (lcm / other.denom);
    return Rational{n, lcm};
  }
  Rational operator-(Rational const &other) const {
    auto lcm = std::lcm(denom, other.denom);
    auto n = num * (lcm / denom) - other.num * (lcm / other.denom);
    return Rational{n, lcm};
  }
  Rational operator*(Rational const &other) const {
    return Rational{num * other.num, denom * other.denom};
  }
  Rational operator/(Rational const &other) const {
    return Rational{num * other.denom, denom * other.num};
  }

  std::partial_ordering operator<=>(Rational const &other) const {
    return ((*this) - other).normalized().num <=> 0;
  }

  bool operator==(Rational const &other) const {
    auto this_norm = this->normalized();
    auto other_norm = other.normalized();
    return (this_norm.num == other_norm.num) &&
           (this_norm.denom == other_norm.denom);
  }

  bool operator!=(Rational const &other) const { return !(*this == other); }

  Rational normalized() const {
    auto sign = ((num < 0) == (denom < 0)) ? 1 : -1;
    auto gcd = std::gcd(std::abs(num), std::abs(denom));

    return Rational{sign * std::abs(num) / gcd, std::abs(denom) / gcd};
  }
};

} // namespace util

namespace llvm {
template <> struct format_provider<util::Rational> {
  static void format(const util::Rational &V, raw_ostream &Stream,
                     StringRef Style) {
    Stream << V.num << "/" << V.denom;
  }
};
} // namespace llvm

#endif