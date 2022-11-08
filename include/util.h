// Copyright 2012 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_UTIL_H_
#define GESTURES_UTIL_H_

#include <map>
#include <set>

#include <math.h>

#include "include/gestures.h"
#include "include/interpreter.h"

namespace gestures {

inline bool FloatEq(float a, float b) {
  return fabsf(a - b) <= 1e-5;
}

inline bool DoubleEq(float a, float b) {
  return fabsf(a - b) <= 1e-8;
}

// Returns the square of the distance between the two contacts.
template<typename ContactTypeA, typename ContactTypeB>
float DistSq(const ContactTypeA& finger_a, const ContactTypeB& finger_b) {
  float dx = finger_a.position_x - finger_b.position_x;
  float dy = finger_a.position_y - finger_b.position_y;
  return dx * dx + dy * dy;
}
template<typename ContactType>  // UnmergedContact or FingerState
float DistSqXY(const ContactType& finger_a, float pos_x, float pos_y) {
  float dx = finger_a.position_x - pos_x;
  float dy = finger_a.position_y - pos_y;
  return dx * dx + dy * dy;
}

template<typename ContactType>
int CompareX(const void* a_ptr, const void* b_ptr) {
  const ContactType* a = *static_cast<const ContactType* const*>(a_ptr);
  const ContactType* b = *static_cast<const ContactType* const*>(b_ptr);
  return a->position_x < b->position_x ? -1 : a->position_x > b->position_x;
}

template<typename ContactType>
int CompareY(const void* a_ptr, const void* b_ptr) {
  const ContactType* a = *static_cast<const ContactType* const*>(a_ptr);
  const ContactType* b = *static_cast<const ContactType* const*>(b_ptr);
  return a->position_y < b->position_y ? -1 : a->position_y > b->position_y;
}

inline float DegToRad(float degrees) {
  return M_PI * degrees / 180.0;
}

template<typename Map, typename Key> static inline
bool MapContainsKey(const Map& the_map, const Key& the_key) {
  return the_map.find(the_key) != the_map.end();
}

// Removes any ids from the map that are not finger ids in hs.
// This implementation supports returning removed elements for
// further processing.
template<typename Data>
void RemoveMissingIdsFromMap(std::map<short, Data>* the_map,
                             const HardwareState& hs,
                             std::map<short, Data>* removed) {
  removed->clear();
  for (typename std::map<short, Data>::const_iterator it =
      the_map->begin(); it != the_map->end(); ++it)
    if (!hs.GetFingerState((*it).first))
      (*removed)[it->first] = it->second;
  for (typename std::map<short, Data>::const_iterator it =
      removed->begin(); it != removed->end(); ++it)
    the_map->erase(it->first);
}

// Removes any ids from the map that are not finger ids in hs.
template<typename Data>
void RemoveMissingIdsFromMap(std::map<short, Data>* the_map,
                             const HardwareState& hs) {
  std::map<short, Data> removed;
  RemoveMissingIdsFromMap(the_map, hs, &removed);
}

// Removes any ids from the set that are not finger ids in hs.
static inline
void RemoveMissingIdsFromSet(std::set<short>* the_set,
                             const HardwareState& hs) {
  short old_ids[the_set->size() + 1];
  size_t old_ids_len = 0;
  for (typename std::set<short>::const_iterator it = the_set->begin();
       it != the_set->end(); ++it)
    if (!hs.GetFingerState(*it))
      old_ids[old_ids_len++] = *it;
  for (size_t i = 0; i < old_ids_len; i++)
    the_set->erase(old_ids[i]);
}

template<typename Set, typename Elt>
inline bool SetContainsValue(const Set& the_set,
                             const Elt& elt) {
  return the_set.find(elt) != the_set.end();
}

}  // namespace gestures

#endif  // GESTURES_UTIL_H_
