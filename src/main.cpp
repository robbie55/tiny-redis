#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "net/server.h"

namespace {

  tinyredis::Server* gServer{nullptr};

  void onSignal(int /*sig*/) {
    if (gServer != nullptr) {
      gServer->stop();
    }
  }

  void usage() {
    std::cerr << "usage: tiny-redis-server [--bind ADDR] [--port N] [--maxkeys N] "
                 "[--buckets N]\n";
  }

}  // namespace

int main(int argc, char** argv) {
  tinyredis::Server::Config cfg;

  for (int i{1}; i < argc; ++i) {
    const std::string arg{argv[i]};
    const bool hasValue{i + 1 < argc};
    if (arg == "--bind" && hasValue) {
      cfg.bindAddr = argv[++i];
    } else if (arg == "--port" && hasValue) {
      cfg.port = static_cast<std::uint16_t>(std::strtoul(argv[++i], nullptr, 10));
    } else if (arg == "--maxkeys" && hasValue) {
      cfg.store.maxKeys = static_cast<std::size_t>(std::strtoull(argv[++i], nullptr, 10));
    } else if (arg == "--buckets" && hasValue) {
      cfg.store.initialBuckets = static_cast<std::size_t>(std::strtoull(argv[++i], nullptr, 10));
    } else {
      usage();
      return 2;
    }
  }

  // A client that disappears mid-write must not take the process with it. Writes also pass
  // MSG_NOSIGNAL, but ignoring SIGPIPE outright removes the whole class of surprise.
  std::signal(SIGPIPE, SIG_IGN);

  try {
    tinyredis::Server server{cfg};
    gServer = &server;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    std::cout << "tiny-redis listening on " << cfg.bindAddr << ':' << cfg.port << " (max keys "
              << server.store().maxKeys() << ")\n"
              << std::flush;
    server.run();
  } catch (const std::exception& e) {
    std::cerr << "fatal: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
