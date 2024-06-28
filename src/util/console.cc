#include "console.h"

#include <iostream>

#include "client_thread.h"

namespace vraft {

Console::Console(const std::string &name)
    : start(false),
      name_(name),
      prompt_("(" + name + ")> "),
      wait_result_(1),
      dest_("0.0.0.0:0") {}

Console::Console(const std::string &name, const HostPort &dest)
    : start(false),
      name_(name),
      prompt_("(" + name + ")> "),
      wait_result_(1),
      dest_(dest) {
  client_thread_ = std::make_shared<ClientThread>(name_, false);
  EventLoopSPtr loop = client_thread_->LoopPtr();

  TcpOptions to;
  TcpClientSPtr tcp_client =
      std::make_shared<vraft::TcpClient>(loop, name_, dest, to);
  tcp_client->set_close_cb(std::bind(&vraft::ClientThread::ServerCloseCountDown,
                                     client_thread_.get()));
  tcp_client->set_on_message_cb(std::bind(
      &Console::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
  client_thread_->AddClient(tcp_client);
}

int32_t Console::Run() {
  if (client_thread_) {
    client_thread_->Start();
  }

  start.store(true);
  while (start.load()) {
    // reset
    Reset();

    // get command
    std::cout << prompt_;
    if (!std::getline(std::cin, cmd_line_)) {
      // EOF or error input
      std::cout << "error input or eof ..." << std::endl;
      break;
    }

    if (cmd_line_ == "") {
      continue;
    }

    // parse command
    int32_t rv = Parse(cmd_line_);
    if (rv != 0) {
      continue;
    }

    // execute command and wait for result
    rv = Execute();
    WaitResult();

    // show result
    std::cout << result_ << std::endl;
  }

  return 0;
}

void Console::Stop() {
  start.store(false);
  if (client_thread_) {
    client_thread_->Stop();
    client_thread_->Join();
  }
}

void Console::Reset() {
  result_.clear();
  cmd_line_.clear();
  wait_result_.Reset();
}

void Console::set_result(const std::string &result) { result_ = result; }

std::string Console::cmd_line() const { return cmd_line_; }

void Console::WaitResult() { wait_result_.Wait(); }

void Console::ResultReady() { wait_result_.CountDown(); }

void Console::Send(std::string &msg) {
  if (client_thread_) {
    TcpClientSPtr client = client_thread_->GetClient(dest_);
    if (client) {
      EventLoopSPtr loop = client_thread_->LoopPtr();
      loop->RunFunctor(std::bind(&TcpClient::CopySend, client.get(),
                                 msg.c_str(), msg.size()));
    }
  }
}

}  // namespace vraft
