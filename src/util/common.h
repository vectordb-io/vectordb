#ifndef VRAFT_COMMON_H_
#define VRAFT_COMMON_H_

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace vraft {

#define MAX_QUEUE_SIZE (4096)

#define JSON_TAB 2

using RaftIndex = uint32_t;
using RaftTerm = uint64_t;

using Functor = std::function<void()>;
using FunctorFinish = std::function<void(int32_t)>;

using TracerCb = std::function<void(std::string)>;

using SendFunc =
    std::function<int32_t(uint64_t dest, const char *buf, unsigned int size)>;

class Console;
using ConsoleSPtr = std::shared_ptr<Console>;
using ConsoleUPtr = std::unique_ptr<Console>;
using ConsoleWPtr = std::weak_ptr<Console>;

class EventLoop;
using EventLoopSPtr = std::shared_ptr<EventLoop>;
using EventLoopUPtr = std::unique_ptr<EventLoop>;
using EventLoopWPtr = std::weak_ptr<EventLoop>;

class AsyncQueue;
using AsyncQueueSPtr = std::shared_ptr<AsyncQueue>;
using AsyncQueueUPtr = std::unique_ptr<AsyncQueue>;
using AsyncQueueWPtr = std::weak_ptr<AsyncQueue>;

class AsyncStop;
using AsyncStopSPtr = std::shared_ptr<AsyncStop>;
using AsyncStopUPtr = std::unique_ptr<AsyncStop>;
using AsyncStopWPtr = std::weak_ptr<AsyncStop>;

class Timer;
using TimerSPtr = std::shared_ptr<Timer>;
using TimerUPtr = std::unique_ptr<Timer>;
using TimerWPtr = std::weak_ptr<Timer>;

using TimerId = int64_t;
using TimerMap = std::map<TimerId, TimerSPtr>;
using TimerFunctor = std::function<void(Timer *)>;

class TcpConnection;
using TcpConnectionSPtr = std::shared_ptr<TcpConnection>;
using TcpConnectionUPtr = std::unique_ptr<TcpConnection>;
using TcpConnectionWPtr = std::weak_ptr<TcpConnection>;

class TcpServer;
using TcpServerSPtr = std::shared_ptr<TcpServer>;
using TcpServerUPtr = std::unique_ptr<TcpServer>;
using TcpServerWPtr = std::weak_ptr<TcpServer>;

class TcpClient;
using TcpClientSPtr = std::shared_ptr<TcpClient>;
using TcpClientUPtr = std::unique_ptr<TcpClient>;
using TcpClientWPtr = std::weak_ptr<TcpClient>;

class RaftServer;
using RaftServerSPtr = std::shared_ptr<RaftServer>;
using RaftServerUPtr = std::unique_ptr<RaftServer>;
using RaftServerWPtr = std::weak_ptr<RaftServer>;

class Remu;
using RemuSPtr = std::shared_ptr<Remu>;
using RemuUPtr = std::unique_ptr<Remu>;
using RemuWPtr = std::weak_ptr<Remu>;

class Raft;
using RaftSPtr = std::shared_ptr<Raft>;
using RaftUPtr = std::unique_ptr<Raft>;
using RaftWPtr = std::weak_ptr<Raft>;

class StateMachine;
using StateMachineSPtr = std::shared_ptr<StateMachine>;
using StateMachineUPtr = std::unique_ptr<StateMachine>;
using StateMachineWPtr = std::weak_ptr<StateMachine>;

class WorkThread;
using WorkThreadSPtr = std::shared_ptr<WorkThread>;
using WorkThreadUPtr = std::unique_ptr<WorkThread>;
using WorkThreadWPtr = std::weak_ptr<WorkThread>;

class WorkThreadPool;
using WorkThreadPoolSPtr = std::shared_ptr<WorkThreadPool>;
using WorkThreadPoolUPtr = std::unique_ptr<WorkThreadPool>;
using WorkThreadPoolWPtr = std::weak_ptr<WorkThreadPool>;

class LoopThread;
using LoopThreadSPtr = std::shared_ptr<LoopThread>;
using LoopThreadUPtr = std::unique_ptr<LoopThread>;
using LoopThreadWPtr = std::weak_ptr<LoopThread>;

class ServerThread;
using ServerThreadSPtr = std::shared_ptr<ServerThread>;
using ServerThreadUPtr = std::unique_ptr<ServerThread>;
using ServerThreadWPtr = std::weak_ptr<ServerThread>;

class CountDownLatch;
using CountDownLatchSPtr = std::shared_ptr<CountDownLatch>;
using CountDownLatchUPtr = std::unique_ptr<CountDownLatch>;
using CountDownLatchWPtr = std::weak_ptr<CountDownLatch>;

class ClientThread;
using ClientThreadSPtr = std::shared_ptr<ClientThread>;
using ClientThreadUPtr = std::unique_ptr<ClientThread>;
using ClientThreadWPtr = std::weak_ptr<ClientThread>;

}  // namespace vraft

#endif
