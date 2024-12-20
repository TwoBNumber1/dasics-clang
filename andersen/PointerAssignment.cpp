#include "AndersonPointsToAnalysis.h"

#include <llvm/Support/Casting.h>

namespace llvm {

namespace anderson {

static size_t CombineHash(size_t lhs, size_t rhs) noexcept {
  lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
  return lhs;
}

size_t PointerAssignedElementPtr::GetHashCode() const noexcept {
  auto baseHash = PointerAssignedPointerBase::GetHashCode();
  for (const auto &index : _indexSequence) {
    auto indexHash = std::hash<size_t> { }(index.index());
    baseHash = CombineHash(baseHash, indexHash);
  }
  return baseHash;
}

bool PointerAssignedElementPtr::operator==(const PointerAssignment &rhs) const noexcept {
  if (rhs.kind() != PointerAssignmentKind::AssignedElementPtr) {
    return false;
  }

  auto rhsCasted = llvm::cast<PointerAssignedElementPtr>(rhs);
  return pointer() == rhsCasted.pointer() && _indexSequence == rhsCasted._indexSequence;
}

} // namespace anderson

} // namespace llvm