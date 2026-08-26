#include "net/server.h"

#include "core/clock.h"
#include "core/todo.h"

namespace tinyredis {

  Server::Acceptor::Acceptor(int fd, Server& server) noexcept : IoHandler(fd), server_(server) {}

  void Server::Acceptor::onReadable() { unimplemented("Server::Acceptor::onReadable"); }

  void Server::Acceptor::onWritable() {}

  Server::Server(const Config& /*cfg*/) { unimplemented("Server::Server"); }

  Server::~Server() = default;

  void Server::run() { unimplemented("Server::run"); }

  void Server::stop() noexcept { unimplemented("Server::stop"); }

  Store& Server::store() noexcept { unimplemented("Server::store"); }

  EventLoop& Server::loop() noexcept { unimplemented("Server::loop"); }

  std::int64_t Server::now() noexcept { unimplemented("Server::now"); }

  void Server::retire(Connection& /*c*/) { unimplemented("Server::retire"); }

  std::size_t Server::connectionCount() const noexcept { unimplemented("Server::connectionCount"); }

  void Server::acceptPending() { unimplemented("Server::acceptPending"); }

  void Server::reapRetired() { unimplemented("Server::reapRetired"); }

}  // namespace tinyredis
