#include "asio.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>

using asio::ip::tcp;
using std::endl;

//----------------------------------------------------------------------

class auth_server {
public:
  auth_server(asio::io_context &io_context, const tcp::endpoint &endpoint)
      : acceptor_(io_context, endpoint) {
    std::ifstream db("../db.txt");
    if (db.is_open()) {
      std::string login;
      while (std::getline(db, login, ':')) {
        std::string password;
        std::getline(db, password);

        user_db[std::move(login)] = password;
      }
      db.close();
    }

    do_accept();
  }

private:
  void do_accept() {

    acceptor_.async_accept([this](std::error_code ec, tcp::socket socket) {
      if (!ec) {
        try {
          std::array<char, 5> header;
          socket.read_some(asio::buffer(header, 5));
          std::array<char, 128> body;
          std::size_t len = socket.read_some(asio::buffer(body));

          read_msg(header.data(), body.data(), std::move(socket), len);
        } catch (std::exception &e) {
          std::cerr << e.what() << endl;
        }
      }

      do_accept();
    });
  }

  void read_msg(std::string header, std::string body, tcp::socket socket,
                std::size_t len) {
    body = body.substr(0, len);

    while (body.find(' ') != body.npos)
      body.erase(std::find(body.begin(), body.end(), ' '));

    if (header.find("REG") != header.npos) {
      register_new(std::move(body), std::move(socket));
    } else if (header.find("AUTH") != header.npos) {
      write_hash(std::move(body), std::move(socket));
    } else if (header.find("CHECK") != header.npos) {
      check_token(std::move(body), std::move(socket));
    }
  }

  void register_new(std::string &&body, tcp::socket socket) {

    std::string login = body.substr(0, body.find_first_of(':'));
    std::string password =
        body.substr(body.find_first_of(':') + 1, body.size());
    std::string hash_pass = std::to_string(std::hash<std::string>{}(password));

    if (user_db.find(login) == user_db.end()) {
      std::ofstream db("../db.txt", std::ios::app);
      db << login << ':' << hash_pass << '\n';
      user_db[std::move(login)] = hash_pass;
      socket.write_some(asio::buffer("OK   "));
      db.close();
    }
    socket.close();
  }

  //---------------------------------------------------------------
  // only game server part
  void check_token(std::string &&body, tcp::socket socket) {
    std::string login = body.substr(0, body.find_first_of(':'));
    std::string token = body.substr(body.find_first_of(':') + 1, body.size());

    if (last_token.find(login) != last_token.end()) {
      if (last_token.at(login) == token) {
        asio::write(socket, asio::buffer("OK   "));
        socket.close();
        return;
      }
    }

    asio::write(socket, asio::buffer("ERR  Wrong TOKEN\n"));
    socket.close();
  }

  // End
  //---------------------------------------------------------------

  void write_hash(std::string &&body, tcp::socket socket) {

    std::string login = body.substr(0, body.find_first_of(':'));
    std::string password =
        body.substr(body.find_first_of(':') + 1, body.size());

    std::string hash_pass = std::to_string(std::hash<std::string>{}(password));

    if (user_db.find(login) != user_db.end()) {
      if (user_db.at(login) == hash_pass) {
        last_token[login] =
            std::move(std::string(hash_pass.crbegin(), hash_pass.crend()));
        asio::write(socket, asio::buffer("OK   " + last_token.at(login)));
      } else {
        std::cout << user_db.at(login) << ' ' << hash_pass << endl;
        asio::write(socket, asio::buffer("ERR  Wrong password\n"));
      }
    }
    socket.close();
  }

  void update_file() {
    std::ofstream db("../db.txt");
    for (auto &udb : user_db) {
      db << udb.first << ':' << udb.second << '\n';
    }
    db.close();
  }

  tcp::acceptor acceptor_;
  std::map<std::string, std::string> user_db;
  std::map<std::string, std::string> last_token;
};

//----------------------------------------------------------------------

int main(int argc, char *argv[]) {
  try {
    asio::io_context io_context;
    tcp::endpoint endpoint(tcp::v4(), 8000);
    auth_server server(io_context, endpoint);
    io_context.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
