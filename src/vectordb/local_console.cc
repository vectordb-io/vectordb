#include "local_console.h"

#include "clock.h"
#include "parser.h"
#include "util.h"
#include "vdb_engine.h"
#include "version.h"

namespace vectordb {

LocalConsole::LocalConsole(const std::string &name, const std::string &path)
    : Console(name) {
  vdb_ = std::make_shared<VdbEngine>(path);
  assert(vdb_);
}

int32_t LocalConsole::Parse(const std::string &cmd_line) {
  Clear();
  parser_ = std::make_shared<Parser>(cmd_line);
  return 0;
}

int32_t LocalConsole::Execute() {
  if (parser_) {
    switch (parser_->cmd()) {
      case kCmdError: {
        Error();
        break;
      }

      case kCmdHelp: {
        Help();
        break;
      }

      case kCmdVersion: {
        Version();
        break;
      }

      case kCmdQuit: {
        Quit();
        break;
      }

      case kCmdMeta: {
        Meta();
        break;
      }

      case kCmdPut: {
        Put();
        break;
      }

      case kCmdGet: {
        Get();
        break;
      }

      case kCmdDelete: {
        Delete();
        break;
      }

      case kCmdGetKNN: {
        GetKNN();
        break;
      }

      case kCmdLoad: {
        Load();
        break;
      }

      case kCmdCreateTable: {
        CreateTable();
        break;
      }

      case kCmdBuildIndex: {
        BuildIndex();
        break;
      }

      case kDescTable: {
        DescTable();
        break;
      }

      case kDescPartition: {
        DescPartition();
        break;
      }

      case kDescDescReplica: {
        DescDescReplica();
        break;
      }

      case kDescDescEngine: {
        DescDescEngine();
        break;
      }

      case kShowTables: {
        ShowTables();
        break;
      }

      case kShowPartitions: {
        ShowPartitions();
        break;
      }

      case kShowReplicas: {
        ShowReplicas();
        break;
      }

      default:
        Error();
        break;
    }
    ResultReady();
  }

  return 0;
}

void LocalConsole::Clear() { parser_.reset(); }

void LocalConsole::Help() { set_result(HelpStr()); }

void LocalConsole::Error() {
  std::string err = "error command";
  set_result(err);
}

void LocalConsole::Quit() {
  Clear();
  Console::Stop();
}

void LocalConsole::Version() { set_result(VECTORDB_VERSION); }

void LocalConsole::Meta() {
  if (vdb_) {
    set_result(vdb_->ToJsonString(false, false));
  } else {
    set_result("error");
  }
}

void LocalConsole::CreateTable() {
  if (vdb_ && parser_) {
    vectordb::AddTableParam param;
    param.name = parser_->name();
    param.partition_num = parser_->partition_num();
    param.replica_num = parser_->replica_num();
    param.dim = parser_->dim();
    int32_t rv = vdb_->AddTable(param);
    if (rv == 0) {
      set_result("ok");
    } else {
      set_result("error");
    }
  }
}

void LocalConsole::Load() {
  if (vdb_ && parser_) {
    int32_t rv = vdb_->Load(parser_->table(), parser_->file());
    if (rv == 0) {
      set_result("ok");
    } else {
      set_result("error");
    }
  }
}

void LocalConsole::GetKNN() {
  if (vdb_ && parser_) {
    VecResults results;
    if (parser_->key() != "") {
      int32_t rv = vdb_->GetKNN(parser_->table(), parser_->key(),
                                results.results, parser_->limit());
      if (rv == 0) {
        set_result(results.ToPrintString());
      } else {
        set_result("error");
      }
    } else {
      int32_t rv = vdb_->GetKNN(parser_->table(), parser_->vec(),
                                results.results, parser_->limit());
      if (rv == 0) {
        set_result(results.ToPrintString());
      } else {
        set_result("error");
      }
    }
  }
}

void LocalConsole::BuildIndex() {
  if (vdb_ && parser_) {
    TableSPtr table = vdb_->meta()->GetTable(parser_->table());
    if (table) {
      AddIndexParam param;
      param.timestamp = vraft::Clock::NSec();
      param.dim = table->dim;
      param.index_type = kIndexAnnoy;
      param.distance_type = kCosine;
      param.annoy_tree_num = parser_->annoy_tree_num();
      int32_t rv = vdb_->AddIndex(parser_->table(), param);
      if (rv == 0) {
        set_result("ok");
      } else {
        set_result("error");
      }
    }
  }
}

void LocalConsole::ShowTables() {
  if (vdb_ && parser_) {
    Names names;
    vdb_->meta()->Tables(names);
    set_result(names.ToJsonString(false, false));
  }
}

void LocalConsole::ShowPartitions() {
  if (vdb_ && parser_) {
    Names names;
    vdb_->meta()->Partitions(names);
    set_result(names.ToJsonString(false, false));
  }
}

void LocalConsole::ShowReplicas() {
  if (vdb_ && parser_) {
    Names names;
    vdb_->meta()->Replicas(names);
    set_result(names.ToJsonString(false, false));
  }
}

void LocalConsole::DescTable() {
  if (vdb_ && parser_) {
    TableSPtr sptr = vdb_->meta()->GetTable(parser_->name());
    if (sptr) {
      set_result(sptr->ToJsonString(false, false));
    }
  }
}

void LocalConsole::DescPartition() {
  if (vdb_ && parser_) {
    PartitionSPtr sptr = vdb_->meta()->GetPartition(parser_->name());
    if (sptr) {
      set_result(sptr->ToJsonString(false, false));
    }
  }
}

void LocalConsole::DescDescReplica() {
  if (vdb_ && parser_) {
    ReplicaSPtr sptr = vdb_->meta()->GetReplica(parser_->name());
    if (sptr) {
      set_result(sptr->ToJsonString(false, false));
    }
  }
}

void LocalConsole::DescDescEngine() {
  if (vdb_ && parser_) {
    VEngineSPtr sptr = vdb_->GetVEngine(parser_->name());
    if (sptr) {
      set_result(sptr->ToJsonString(false, false));
    }
  }
}

void LocalConsole::Get() {
  if (vdb_ && parser_) {
    VecObj vo;
    int32_t rv = vdb_->Get(parser_->table(), parser_->key(), vo);
    if (rv == 0) {
      set_result(vo.ToJsonString(false, false));
    } else {
      set_result("error");
    }
  }
}

void LocalConsole::Put() {
  if (vdb_ && parser_) {
    VecValue vv;
    vv.vec.data = parser_->vec();
    vv.attach_value = parser_->attach_value();
    int32_t rv = vdb_->Put(parser_->table(), parser_->key(), vv);
    if (rv == 0) {
      set_result("ok");
    } else {
      set_result("error");
    }
  }
}

void LocalConsole::Delete() {
  if (vdb_ && parser_) {
    int32_t rv = vdb_->Delete(parser_->table(), parser_->key());
    if (rv == 0) {
      set_result("ok");
    } else {
      set_result("error");
    }
  }
}

}  // namespace vectordb
