#include "vstore_console.h"

#include "client_request.h"
#include "kv.h"
#include "util.h"
#include "vstore_msg.h"

namespace vstore {

int32_t VstoreConsole::Parse(const std::string &cmd_line) {
  Clear();

  std::string cmd_line2 = cmd_line;
  vraft::DelHead(cmd_line2, " ");
  vraft::DelTail(cmd_line2, " ;");

  std::vector<std::string> result;
  vraft::Split(cmd_line2, ' ', result);

  cmd_ = result[0];

  if (cmd_ == "get" && result.size() == 2) {
    key_ = result[1];
  }

  if (cmd_ == "set" && result.size() == 3) {
    key_ = result[1];
    value_ = result[2];
  }

  if (cmd_ == "leader" && result.size() == 2) {
    leader_transfer_ = result[1];
  }

  return 0;
}

int32_t VstoreConsole::Execute() {
  if (cmd_ == "help") {
    set_result(HelpString());
    ResultReady();

  } else if (cmd_ == "set") {
    vraft::ClientRequest msg;
    msg.uid = UniqId(&msg);
    msg.cmd = vraft::kCmdPropose;
    vraft::KV kv;
    kv.key = key_;
    kv.value = value_;
    kv.ToString(msg.data);

    std::string body_str;
    int32_t bytes = msg.ToString(body_str);

    vraft::MsgHeader header;
    header.body_bytes = bytes;
    header.type = vraft::kClientRequet;
    std::string header_str;
    header.ToString(header_str);
    header_str.append(std::move(body_str));
    Send(header_str);

    set_result("ok");
    ResultReady();

  } else if (cmd_ == "get") {
    vraft::ClientRequest msg;
    msg.uid = UniqId(&msg);
    msg.cmd = vraft::kCmdGet;
    msg.data = key_;

    std::string body_str;
    int32_t bytes = msg.ToString(body_str);

    vraft::MsgHeader header;
    header.body_bytes = bytes;
    header.type = vraft::kClientRequet;
    std::string header_str;
    header.ToString(header_str);
    header_str.append(std::move(body_str));
    Send(header_str);

  } else if (cmd_ == "leader") {
    vraft::ClientRequest msg;
    msg.uid = UniqId(&msg);
    msg.cmd = vraft::kCmdLeaderTransfer;
    msg.data = leader_transfer_;

    std::string body_str;
    int32_t bytes = msg.ToString(body_str);

    vraft::MsgHeader header;
    header.body_bytes = bytes;
    header.type = vraft::kClientRequet;
    std::string header_str;
    header.ToString(header_str);
    header_str.append(std::move(body_str));
    Send(header_str);

    set_result("ok");
    ResultReady();
  }

  return 0;
}

void VstoreConsole::OnMessage(const vraft::TcpConnectionSPtr &conn,
                              vraft::Buffer *buf) {
  set_result(std::string(buf->Peek()));
  buf->RetrieveAll();
  ResultReady();
}

void VstoreConsole::Clear() {
  cmd_.clear();
  key_.clear();
  value_.clear();
  leader_transfer_.clear();
}

std::string VstoreConsole::HelpString() {
  std::string help_str;
  help_str.append("help \n");
  help_str.append("set kk vv \n");
  help_str.append("get kk \n");
  help_str.append("leader 127.0.0.1:9001#0 \n");
  return help_str;
}

}  // namespace vstore
