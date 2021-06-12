#include "domino_client.hpp"
#include <asio.hpp>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>

using asio::ip::tcp;

int main(int argc, char *argv[]) {
  try {

    asio::io_context io_context;
    tcp::resolver resolver(io_context);
    // tcp::resolver::results_type endpoints = resolver.resolve(argv[1],
    // "daytime");
    std::string defaultIP{"127.0.0.1"};
    std::string defailtPort{"8080"};
    tcp::socket socket(io_context);

    bool log_in = false;
    std::string login;
    std::string password;

    while (!log_in) {
      printf("[1] - Register new user\n"
             "[2] - Log in\n");
      int choice;
      std::cin >> choice;
      switch (choice) {
      case 1: {
        printf("Input login: ");
        std::cin >> login;
        printf("Input password: ");
        std::cin >> password;

        asio::connect(socket, resolver.resolve(defaultIP, "8000"));
        asio::write(socket, asio::buffer(("REG  " + login + ':' + password)));
        std::array<char, 20> buf;
        socket.read_some(asio::buffer(buf));

        printf("%s", buf.data());
        socket.close();
      } break;
      case 2: {
        printf("Input login: ");
        std::cin >> login;
        printf("Input password: ");
        std::cin >> password;
        asio::connect(socket, resolver.resolve(defaultIP, "8000"));
        socket.write_some(asio::buffer(("AUTH " + login + ':' + password)));
        std::array<char, 25> buf;
        std::error_code ec;
        std::size_t len = socket.read_some(asio::buffer(buf), ec);
        std::string head, body;
        std::string answer{buf.data()};
        head = answer.substr(0, 5);
        body = answer.substr(5, len);
        if (head.find("OK") != head.npos) {
          log_in = true;
          password = body.substr(0, len - 5);
        }
        socket.close();
      } break;
      default:
        printf("Wrong choice\n");
        break;
      }
    }

    asio::connect(socket, resolver.resolve(argv[1], argv[2]));
    std::error_code ec;
    socket.write_some(asio::buffer(("AUTH " + login + ':' + password)), ec);
    if (ec) {
      std::cout << ec;
      return 0;
    }

    domino d(std::move(socket));
    std::array<char, 11> buf;
    std::size_t len = 0;
    while (d.socket().available() == 0) {
      asio::steady_timer t(io_context, asio::chrono::seconds(1));
      t.wait();
    }
    len = d.socket().read_some(asio::buffer(buf), ec);
    std::cout << "len: " << len << std::endl;
    std::cout.write(buf.data(), len);

    std::cout << '\n';
    {
      std::string returned{buf.data()};
      if (returned.find("Auth") != returned.npos) {
        return 0;
      }
      if (returned.find("full") != returned.npos) {
        return 0;
      }
    }
    bool game_is_over = false;

    while (!game_is_over) {
      std::array<char, 128> buff;
      len = d.socket().read_some(asio::buffer(buff), ec);
      std::string returned{buff.data()};
      if (returned.find("END") != returned.npos) {
        break;
      }
      std::cout.write(buff.data(), len);
      printf("\n[1] - place domino <num>\n"
             "[2] - show dominoes in hand\n"
             "[3] - get 1 more domino\n"
             "[4] - skip step\n");
      bool step_end = false;
      while (!step_end) {
        int choice;
        std::cin >> choice;
        switch (choice) {
        case 1: {
          int i;
          printf("Element to send: ");
          std::cin >> i;
          if (!d.send_domino(i)) {
            printf("U cant send this, try smth another\n");
          } else {
            printf("OK\n");
            step_end = true;
          }
        } break;
        case 2: {
          d.print_dominoes();
        } break;
        case 3: {
          step_end = !d.request_domino();
        } break;
        case 4:
          step_end = d.send_skip();
          break;
        default:
          printf("Wrong choice\n");
          break;
        }
      }
    }
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
