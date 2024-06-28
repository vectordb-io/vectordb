#include "vdb_console.h"

#include "msg_version.h"
#include "msg_version_reply.h"
#include "util.h"
#include "vdb_message.h"

namespace vectordb {

int32_t VdbConsole::Parse(const std::string &cmd_line) {
  Clear();
  vraft::ConvertStringToArgcArgv(cmd_line, &argc_, &argv_);
  options_ = std::make_shared<cxxopts::Options>(std::string(argv_[0]));
  parse_result_ = options_->parse(argc_, argv_);
  cmd_ = argv_[0];
  vraft::ToLower(cmd_);
  return 0;
}

int32_t VdbConsole::Execute() {
  if (cmd_ == "help") {
    Help();
    ResultReady();

  } else if (cmd_ == "version") {
    Version();

  } else if (cmd_ == "quit") {
    Quit();
    ResultReady();

  } else {
    Error();
    ResultReady();
  }

  return 0;
}

void VdbConsole::OnMessage(const vraft::TcpConnectionSPtr &conn,
                           vraft::Buffer *buf) {
  if (buf->ReadableBytes() >= static_cast<int32_t>(sizeof(vraft::MsgHeader))) {
    int32_t body_bytes = buf->PeekInt32();
    if (buf->ReadableBytes() >=
        static_cast<int32_t>(sizeof(vraft::MsgHeader)) + body_bytes) {
      // parse header
      vraft::MsgHeader header;
      header.FromString(buf->BeginRead(), sizeof(vraft::MsgHeader));
      buf->Retrieve(sizeof(vraft::MsgHeader));

      // parse body
      switch (header.type) {
        case kVersionReply: {
          MsgVersionReply msg;
          int32_t sz = msg.FromString(buf->BeginRead(), body_bytes);
          assert(sz > 0);
          buf->Retrieve(body_bytes);
          OnVersionReply(msg);
          break;
        }

        default:
          assert(0);
      }
    }
  }

  ResultReady();
}

void VdbConsole::OnVersionReply(const MsgVersionReply &msg) {
  set_result(std::string(msg.version));
}

void VdbConsole::Clear() {
  cmd_.clear();
  options_.reset();
  vraft::FreeArgv(argc_, argv_);
}

void VdbConsole::Help() {
  std::string help = "help\n";
  help.append("quit\n");
  help.append("version");
  set_result(help);
}

void VdbConsole::Error() {
  std::string err = "error command";
  set_result(err);
}

void VdbConsole::Quit() {
  Clear();
  Console::Stop();
}

void VdbConsole::Version() {
  MsgVersion msg;
  std::string msg_str;
  int32_t bytes = msg.ToString(msg_str);

  vraft::MsgHeader header;
  header.body_bytes = bytes;
  header.type = kVersion;
  std::string header_str;
  header.ToString(header_str);

  header_str.append(std::move(msg_str));
  Send(header_str);
}

}  // namespace vectordb
