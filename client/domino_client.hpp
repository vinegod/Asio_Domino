#pragma once

#include "asio.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

class domino {
public:
  domino(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}
  asio::ip::tcp::socket &socket() { return socket_; }

  bool send_domino(std::uint16_t i) {
    socket_.write_some(asio::buffer("POST " + std::to_string(i)));
    std::array<char, 128> buf;
    std::size_t len = socket_.read_some(asio::buffer(buf));
    std::string head{buf.data()};
    if (head.find("OK") != head.npos) {
      return 1;
    } else {
      return 0;
    }
  }

  bool request_domino() {
    socket_.write_some(asio::buffer(std::string("GET  ")));
    std::array<char, 128> buf;
    std::size_t len = socket_.read_some(asio::buffer(buf));
    std::string head{buf.data()};
    if (head.find("OK") != head.npos) {
      print_dominoes();
      return true;
    } else {
      printf("Table is empty, try luck next step");
      return false;
    }
  }

  void print_dominoes() {
    std::error_code ec;
    socket_.write_some(asio::buffer(std::string("PRINT")), ec);

    /*asio::io_context new_io;
    asio::steady_timer t(new_io, asio::chrono::seconds(1));
    t.wait();*/

    std::array<char, 5> head;
    std::size_t len = socket_.read_some(asio::buffer(head), ec);
    std::string head_str{head.data()};
    if (head_str.find("OK") != head_str.npos) {

      std::array<char, 128> buff;
      len = socket_.read_some(asio::buffer(buff), ec);
      std::cout.write(buff.data(), len);
      std::cout << '\n';
    } else {
      std::cout << "Smth happend\n";
    }
  }

  bool send_skip() {
    socket_.write_some(asio::buffer(std::string("SKIP ")));
    return true;
  }

private:
  asio::ip::tcp::socket socket_;
};
