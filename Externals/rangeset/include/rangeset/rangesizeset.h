#pragma once

#include <cassert>
#include <cstddef>
#include <map>
#include <type_traits>

namespace HyoutaUtilities {
// Like RangeSet, but additionally stores a map of the ranges sorted by their size, for quickly finding the largest or
// smallest range.
template <typename T> class RangeSizeSet {
private:
  // Key type used in the by-size multimap. Should be a type big enough to hold all possible distances between
  // possible 'from' and 'to'.
  // I'd actually love to just do
  // using SizeT = typename std::conditional<std::is_pointer_v<T>,
  //     std::size_t, typename std::make_unsigned<T>::type>::type;
  // but that's apparently not possible due to the std::make_unsigned<T>::type not existing for pointer types
  // so we'll work around this...
  template <typename U, bool IsPointer> struct GetSizeType { using S = typename std::make_unsigned<U>::type; };
  template <typename U> struct GetSizeType<U, true> { using S = std::size_t; };

public:
  using SizeT = typename GetSizeType<T, std::is_pointer_v<T>>::S;

private:
  // Value type stored in the regular range map.
  struct Value {
    // End point of the range.
    T To;

    // Pointer to the same range in the by-size multimap.
    typename std::multimap<SizeT, typename std::map<T, Value>::iterator, std::greater<SizeT>>::iterator SizeIt;

    Value(T to) : To(to) {}

    bool operator==(const Value& other) const {
      return this->To == other.To;
    }

    bool operator!=(const Value& other) const {
      return !operator==(other);
    }
  };

  using MapT = std::map<T, Value>;
  using SizeMapT = std::multimap<SizeT, typename MapT::iterator, std::greater<SizeT>>;

public:
  struct by_size_const_iterator;

  struct const_iterator {
  public:
    const T& from() const {
      return It->first;
    }

    const T& to() const {
      return It->second.To;
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

    by_size_const_iterator to_size_iterator() {
      return by_size_const_iterator(It->second.SizeIt);
    }

  private:
    typename MapT::const_iterator It;
    const_iterator(typename MapT::const_iterator it) : It(it) {}
    friend class RangeSizeSet;
  };

  struct by_size_const_iterator {
  public:
    const T& from() const {
      return It->second->first;
    }

    const T& to() const {
      return It->second->second.To;
    }

    by_size_const_iterator& operator++() {
      ++It;
      return *this;
    }

    by_size_const_iterator operator++(int) {
      by_size_const_iterator old = *this;
      ++It;
      return old;
    }

    by_size_const_iterator& operator--() {
      --It;
      return *this;
    }

    by_size_const_iterator operator--(int) {
      by_size_const_iterator old = *this;
      --It;
      return old;
    }

    bool operator==(const by_size_const_iterator& rhs) const {
      return this->It == rhs.It;
    }

    bool operator!=(const by_size_const_iterator& rhs) const {
      return !operator==(rhs);
    }

    const_iterator to_range_iterator() {
      return const_iterator(It->second);
    }

  private:
    typename SizeMapT::const_iterator It;
    by_size_const_iterator(typename SizeMapT::const_iterator it) : It(it) {}
    friend class RangeSizeSet;
  };

  // We store iterators internally, so disallow copying.
  RangeSizeSet() = default;
  RangeSizeSet(const RangeSizeSet<T>&) = delete;
  RangeSizeSet(RangeSizeSet<T>&&) = default;
  RangeSizeSet<T>& operator=(const RangeSizeSet<T>&) = delete;
  RangeSizeSet<T>& operator=(RangeSizeSet<T>&&) = default;

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

  by_size_const_iterator erase(by_size_const_iterator it) {
    return by_size_const_iterator(erase_range_by_size(it.It));
  }

  void clear() {
    Map.clear();
    Sizes.clear();
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

  std::size_t by_size_count(const SizeT& key) const {
    return Sizes.count(key);
  }

  by_size_const_iterator by_size_find(const SizeT& key) const {
    return Sizes.find(key);
  }

  std::pair<by_size_const_iterator, by_size_const_iterator> by_size_equal_range(const SizeT& key) const {
    auto p = Sizes.equal_range(key);
    return std::pair<by_size_const_iterator, by_size_const_iterator>(by_size_const_iterator(p.first),
                                                                     by_size_const_iterator(p.second));
  }

  by_size_const_iterator by_size_lower_bound(const SizeT& key) const {
    return Sizes.lower_bound(key);
  }

  by_size_const_iterator by_size_upper_bound(const SizeT& key) const {
    return Sizes.upper_bound(key);
  }

  void swap(RangeSizeSet<T>& other) {
    Map.swap(other.Map);
    Sizes.swap(other.Sizes);
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

  by_size_const_iterator by_size_begin() const {
    return by_size_const_iterator(Sizes.begin());
  }

  by_size_const_iterator by_size_end() const {
    return by_size_const_iterator(Sizes.end());
  }

  by_size_const_iterator by_size_cbegin() const {
    return by_size_const_iterator(Sizes.cbegin());
  }

  by_size_const_iterator by_size_cend() const {
    return by_size_const_iterator(Sizes.cend());
  }

  bool operator==(const RangeSizeSet<T>& other) const {
    return this->Map == other.Map;
  }

  bool operator!=(const RangeSizeSet<T>& other) const {
    return !(*this == other);
  }

private:
  static SizeT calc_size(T from, T to) {
    if constexpr (std::is_pointer_v<T>) {
      // For pointers we don't want pointer arithmetic here, else void* breaks.
      static_assert(sizeof(T) <= sizeof(SizeT));
      return reinterpret_cast<SizeT>(to) - reinterpret_cast<SizeT>(from);
    } else {
      return static_cast<SizeT>(to - from);
    }
  }

  // Assumptions that can be made about the data:
  // - Range are stored in the form [from, to[
  //   That is, the starting value is inclusive, and the end value is exclusive.
  // - 'from' is the map key, 'to' is the map value
  // - 'from' is always smaller than 'to'
  // - Stored ranges never touch.
  // - Stored ranges never overlap.
  MapT Map;

  // The by-size multimap.
  // Key is the size of the range.
  // Value is a pointer to the range in the regular range map.
  // We use std::greater so that Sizes.begin() gives us the largest range.
  SizeMapT Sizes;

  T get_from(typename MapT::iterator it) const {
    return it->first;
  }

  T get_to(typename MapT::iterator it) const {
    return it->second.To;
  }

  T get_from(typename MapT::const_iterator it) const {
    return it->first;
  }

  T get_to(typename MapT::const_iterator it) const {
    return it->second.To;
  }

  typename MapT::iterator insert_range(T from, T to) {
    auto m = Map.emplace(from, to).first;
    m->second.SizeIt = Sizes.emplace(calc_size(from, to), m);
    return m;
  }

  typename MapT::iterator erase_range(typename MapT::iterator it) {
    Sizes.erase(it->second.SizeIt);
    return Map.erase(it);
  }

  typename MapT::const_iterator erase_range(typename MapT::const_iterator it) {
    Sizes.erase(it->second.SizeIt);
    return Map.erase(it);
  }

  typename SizeMapT::const_iterator erase_range_by_size(typename SizeMapT::const_iterator it) {
    Map.erase(it->second);
    return Sizes.erase(it);
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
    it->second.To = to;
    Sizes.erase(it->second.SizeIt);
    it->second.SizeIt = Sizes.emplace(calc_size(get_from(it), to), it);
  }

  void reduce_to(typename MapT::iterator it, T to) {
    assert(get_to(it) > to);
    it->second.To = to;
    Sizes.erase(it->second.SizeIt);
    it->second.SizeIt = Sizes.emplace(calc_size(get_from(it), to), it);
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
