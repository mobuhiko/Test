// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_OBJECT_ID_STATE_MAP_H_
#define SYNC_NOTIFIER_OBJECT_ID_STATE_MAP_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "google/cacheinvalidation/include/types.h"
#include "sync/internal_api/public/base/invalidation_state.h"
#include "sync/internal_api/public/base/model_type_state_map.h"
#include "sync/notifier/invalidation_util.h"

namespace base {
class ListValue;
}  // namespace base

namespace syncer {

typedef std::map<invalidation::ObjectId,
                 InvalidationState,
                 ObjectIdLessThan> ObjectIdStateMap;

// Converts between ObjectIdStateMaps and ObjectIdSets.
ObjectIdSet ObjectIdStateMapToSet(const ObjectIdStateMap& id_payloads);
ObjectIdStateMap ObjectIdSetToStateMap(const ObjectIdSet& ids,
                                       const std::string& payload);

bool ObjectIdStateMapEquals(const ObjectIdStateMap& id_state_map1,
                            const ObjectIdStateMap& id_state_map2);

scoped_ptr<base::ListValue> ObjectIdStateMapToValue(
    const ObjectIdStateMap& model_type_payloads);

bool ObjectIdStateMapFromValue(const base::ListValue& value,
                               ObjectIdStateMap* out);

// Converts between ObjectIdStateMaps and ModelTypeStateMaps.
ModelTypeStateMap ObjectIdStateMapToModelTypeStateMap(
    const ObjectIdStateMap& id_payloads);
ObjectIdStateMap ModelTypeStateMapToObjectIdStateMap(
    const ModelTypeStateMap& type_payloads);

}  // namespace syncer

#endif  // HOME_DCHENG_SRC_CHROMIUM_SRC_SYNC_NOTIFIER_OBJECT_ID_STATE_MAP_H_
