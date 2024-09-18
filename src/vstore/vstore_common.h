#ifndef VSTORE_VSTORE_COMMON_H_
#define VSTORE_VSTORE_COMMON_H_

#include <memory>

namespace vstore {

class Vstore;
using VstoreSPtr = std::shared_ptr<Vstore>;
using VstoreUPtr = std::unique_ptr<Vstore>;
using VstoreWPtr = std::weak_ptr<Vstore>;

class VstoreSm;
using VstoreSmSPtr = std::shared_ptr<VstoreSm>;
using VstoreSmUPtr = std::unique_ptr<VstoreSm>;
using VstoreSmWPtr = std::weak_ptr<VstoreSm>;

}  // namespace vstore

#endif
