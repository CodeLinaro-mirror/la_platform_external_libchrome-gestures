// Copyright 2011 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_LIST_H__
#define GESTURES_LIST_H__

#include <list>
#include "include/logging.h"
#include "include/memory_manager.h"

namespace gestures {

template<typename Elt>
class MemoryManagedList : public std::list<Elt*> {
 public:
  MemoryManagedList() { };
  ~MemoryManagedList() { clear(); }

  void Init(MemoryManager<Elt>* memory_manager) {
    memory_manager_ = memory_manager;
  }

  Elt* PushNewEltBack() {
    AssertWithReturnValue(memory_manager_, NULL);
    Elt* elt = NewElt();
    AssertWithReturnValue(elt, NULL);
    this->push_back(elt);
    return elt;
  }

  Elt* PushNewEltFront() {
    AssertWithReturnValue(memory_manager_, NULL);
    Elt* elt = NewElt();
    AssertWithReturnValue(elt, NULL);
    this->push_front(elt);
    return elt;
  }

  void DeleteFront() {
    AssertWithReturn(memory_manager_);
    memory_manager_->Free(this->front());
    this->pop_front();
  }

  void DeleteBack() {
    AssertWithReturn(memory_manager_);
    memory_manager_->Free(this->pop_back());
  }

  void clear() {
    while (!this->empty())
      DeleteFront();
  }

 private:
  MemoryManager<Elt>* memory_manager_;

  Elt* NewElt() {
    Elt* elt = memory_manager_->Allocate();
    AssertWithReturnValue(elt, NULL);
    elt->next_ = elt->prev_ = NULL;
    return elt;
  }
};


}  // namespace gestures

#endif  // GESTURES_LIST_H__
