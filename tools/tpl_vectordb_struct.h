#ifndef VECTORDB_TPL_H_
#define VECTORDB_TPL_H_

#include <memory>

namespace vectordb {

class Tpl;
using TplSPtr = std::shared_ptr<Tpl>;
using TplUPtr = std::unique_ptr<Tpl>;
using TplWPtr = std::weak_ptr<Tpl>;

struct Tpl {

};

} // namespace vectordb

#endif
