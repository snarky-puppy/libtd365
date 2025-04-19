/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */


#ifndef GRACEFUL_H
#define GRACEFUL_H

#include <boost/asio/io_context.hpp>
#include <boost/asio/awaitable.hpp>
#include <iostream>

struct graceful {
      boost::asio::io_context& io;
      std::atomic<bool> trigger;

      void shutdown() {
        if(!trigger.exchange(true)) {
          std::cout << "Shutting down..." << std::endl;
          io.stop();
        }
      }

      template<typename Awaitable>
      auto operator()(Awaitable&& awaitable) -> boost::asio::awaitable<void> {
        try {
          co_await std::move(awaitable);
        } catch(const std::exception& e) {
          std::cerr << e.what() << std::endl;
        } catch(const boost::system::error_code& e) {
          std::cerr << e << std::endl;
        }

        // any graceful service that finishes will shutdown the io_context
        shutdown();
      }

};

#endif //GRACEFUL_H
