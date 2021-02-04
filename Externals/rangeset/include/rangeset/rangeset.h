#pragma once

#include <cassert>
#include <cstddef>
#include <map>

namespace HyoutaUtilities {
template <typename T> class RangeSet {
private:
  using MapT = std::map<T, T>;

public:
  struct const_iterator {
  public:
    const T& from() const {
      return It->first;
    }

    const T& to() const {
      return It->second;
    }

    const_iterator& operator++() {
      ++It;
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator old = *this;
      ++It;
      return old;
    }

    const_iterator& operator--() {
      --It;
      return *this;
    }

    const_iterator operator--(int) {
      const_iterator old = *this;
      --It;
      return old;
    }

    bool operator==(const const_iterator& rhs) const {
      return this->It == rhs.It;
    }

    bool operator!=(const const_iterator& rhs) const {
      return !operator==(rhs);
    }

  private:
    typename MapT::const_iterator It;
    const_iterator(typename MapT::const_iterator it) : It(it) {}
    friend class RangeSet;
  };

  void insert(T from, T to) {
    if (from >= to)
      return;

    // Start by finding the closest range.
    // upper_bound() returns the closest range whose starting position
    // is greater than 'from'.
    auto bound = Map.upper_bound(from);
    if (bound == Map.end()) {
      // There is no range that starts greater than the given one.
      // This means we have three options:
      // - 1. No range exists yet, this is the first range.
      if (Map.empty()) {
        insert_range(from, to);
        return;
      }

      // - 2. The given range does not overlap the last range.
      --bound;
      if (from > get_to(bound)) {
        insert_range(from, to);
        return;
      }

      // - 3. The given range does overlap the last range.
      maybe_expand_to(bound, to);
      return;
    }

    if (bound == Map.begin()) {
      // The given range starts before any of the existing ones.
      // We must insert this as a new range even if we potentially overlap
      // an existing one as we can't modify a key in a std::map.
      auto inserted = insert_range(from, to);
      merge_from_iterator_to_value(inserted, bound, to);
      return;
    }

    auto abound = bound--;

    // 'bound' now points at the first range in the map that
    // could possibly be affected.

    // If 'bound' overlaps with given range, update bounds object.
    if (get_to(bound) >= from) {
      maybe_expand_to(bound, to);
      auto inserted = bound;
      ++bound;
      merge_from_iterator_to_value(inserted, bound, to);
      return;
    }

    // 'bound' *doesn't* overlap with given range, check next range.

    // If this range overlaps with given range,
    if (get_from(abound) <= to) {
      // insert new range
      auto inserted = insert_range(from, to >= get_to(abound) ? to : get_to(abound));
      // and delete overlaps
      abound = erase_range(abound);
      merge_from_iterator_to_value(inserted, abound, to);
      return;
    }

    // Otherwise, if we come here, then this new range overlaps nothing
    // and must be inserted as a new range.
    insert_range(from, to);
  }

  void erase(T from, T to) {
    if (from >= to)
      return;

    // Like insert(), we use upper_bound to find the closest range.
    auto bound = Map.upper_bound(from);
    if (bound == Map.end()) {
      // There is no range that starts greater than the given one.
      if (Map.empty()) {
        // nothing to do
        return;
      }
      --bound;
      // 'bound' now points at the last range.
      if (from >= get_to(bound)) {
        // Given range is larger than any range that exists, nothing to do.
        return;
      }

      if (to >= get_to(bound)) {
        if (from == get_from(bound)) {
          // Given range fully overlaps last range, erase it.
          erase_range(bound);
          return;
        } else {
          // Given range overlaps end of last range, reduce it.
          reduce_to(bound, from);
          return;
        }
      }

      if (from == get_from(bound)) {
        // Given range overlaps begin of last range, reduce it.
        reduce_from(bound, to);
        return;
      } else {
        // Given range overlaps middle of last range, bisect it.
        bisect_range(bound, from, to);
        return;
      }
    }

    if (bound == Map.begin()) {
      // If we found the first range that means 'from' is before any stored range.
      // This means we can just erase from start until 'to' and be done with it.
      erase_from_iterator_to_value(bound, to);
      return;
    }

    // check previous range
    auto abound = bound--;

    if (from == get_from(bound)) {
      // Similarly, if the previous range starts with the given one, just erase until 'to'.
      erase_from_iterator_to_value(bound, to);
      return;
    }

    // If we come here, the given range may or may not overlap part of the current 'bound'
    // (but never the full range), which means we may need to update the end position of it,
    // or possibly even split it into two.
    if (from < get_to(bound)) {
      if (to < get_to(bound)) {
        // need to split in two
        bisect_range(bound, from, to);
        return;
      } else {
        // just update end
        reduce_to(bound, from);
      }
    }

    // and then just erase until 'to'
    erase_from_iterator_to_value(abound, to);
    return;
  }

  const_iterator erase(const_iterator it) {
    return const_iterator(erase_range(it.It));
  }

  void clear() {
    Map.clear();
  }

  bool contains(T value) const {
    auto it = Map.upper_bound(value);
    if (it == Map.begin())
      return false;
    --it;
    return get_from(it) <= value && value < get_to(it);
  }

  std::size_t size() const {
    return Map.size();
  }

  bool empty() const {
    return Map.empty();
  }

  void swap(RangeSet<T>& other) {
    Map.swap(other.Map);
  }

  const_iterator begin() const {
    return const_iterator(Map.begin());
  }

  const_iterator end() const {
    return const_iterator(Map.end());
  }

  const_iterator cbegin() const {
    return const_iterator(Map.cbegin());
  }

  const_iterator cend() const {
    return const_iterator(Map.cend());
  }

  bool operator==(const RangeSet<T>& other) const {
    return this->Map == other.Map;
  }

  bool operator!=(const RangeSet<T>& other) const {
    return !(*this == other);
  }

private:
  // Assumptions that can be made about the data:
  // - Range are stored in the form [from, to[
  //   That is, the starting value is inclusive, and the end value is exclusive.
  // - 'from' is the map key, 'to' is the map value
  // - 'from' is always smaller than 'to'
  // - Stored ranges never touch.
  // - Stored ranges never overlap.
  MapT Map;

  T get_from(typename MapT::iterator it) const {
    return it->first;
  }

  T get_to(typename MapT::iterator it) const {
    return it->second;
  }

  T get_from(typename MapT::const_iterator it) const {
    return it->first;
  }

  T get_to(typename MapT::const_iterator it) const {
    return it->second;
  }

  typename MapT::iterator insert_range(T from, T to) {
    return Map.emplace(from, to).first;
  }

  typename MapT::iterator erase_range(typename MapT::iterator it) {
    return Map.erase(it);
  }

  typename MapT::const_iterator erase_range(typename MapT::const_iterator it) {
    return Map.erase(it);
  }

  void bisect_range(typename MapT::iterator it, T from, T to) {
    assert(get_from(it) < from);
    assert(get_from(it) < to);
    assert(get_to(it) > from);
    assert(get_to(it) > to);
    assert(from < to);
    T itto = get_to(it);
    reduce_to(it, from);
    insert_range(to, itto);
  }

  typename MapT::iterator reduce_from(typename MapT::iterator it, T from) {
    assert(get_from(it) < from);
    T itto = get_to(it);
    erase_range(it);
    return insert_range(from, itto);
  }

  void maybe_expand_to(typename MapT::iterator it, T to) {
    if (to <= get_to(it))
      return;

    expand_to(it, to);
  }

  void expand_to(typename MapT::iterator it, T to) {
    assert(get_to(it) < to);
    it->second = to;
  }

  void reduce_to(typename MapT::iterator it, T to) {
    assert(get_to(it) > to);
    it->second = to;
  }

  void merge_from_iterator_to_value(typename MapT::iterator inserted, typename MapT::iterator bound, T to) {
    // Erase all ranges that overlap the inserted while updating the upper end.
    while (bound != Map.end() && get_from(bound) <= to) {
      maybe_expand_to(inserted, get_to(bound));
      bound = erase_range(bound);
    }
  }

  void erase_from_iterator_to_value(typename MapT::iterator bound, T to) {
    // Assumption: Given bound starts at or after the 'from' value of the range to erase.
    while (true) {
      // Given range starts before stored range.
      if (to <= get_from(bound)) {
        // Range ends before this range too, nothing to do.
        return;
      }

      if (to < get_to(bound)) {
        // Range ends in the middle of current range, reduce current.
        reduce_from(bound, to);
        return;
      }

      if (to == get_to(bound)) {
        // Range ends exactly with current range, erase current.
        erase_range(bound);
        return;
      }

      // Range ends later than current range.
      // First erase current, then loop to check the range(s) after this one too.
      bound = erase_range(bound);
      if (bound == Map.end()) {
        // Unless that was the last range, in which case there's nothing else to do.
        return;
      }
    }
  }
};
} // namespace HyoutaUtilities
