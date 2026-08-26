#include "net/connection.h"

#include "core/command.h"
#include "core/resp.h"
#include "core/todo.h"
#include "net/server.h"

namespace tinyredis {

  Connection::Connection(int fd, Server& server) : IoHandler(fd), server_(server) {
    unimplemented("Connection::Connection");
  }

  Connection::~Connection() = default;

  void Connection::onReadable() { unimplemented("Connection::onReadable"); }

  void Connection::onWritable() { unimplemented("Connection::onWritable"); }

  bool Connection::closed() const noexcept { unimplemented("Connection::closed"); }

  void Connection::processInput() { unimplemented("Connection::processInput"); }

  void Connection::flushOutput() { unimplemented("Connection::flushOutput"); }

  void Connection::markClosed() { unimplemented("Connection::markClosed"); }

  void Connection::updateInterest() { unimplemented("Connection::updateInterest"); }

}  // namespace tinyredis
