#include "domino_table.hpp"

#include "asio.hpp"

#include <array>
#include <asio/detail/chrono.hpp>
#include <asio/steady_timer.hpp>
#include <iostream>
#include <thread>
#include <utility>

#include <deque>
#include <list>
#include <map>
#include <string>
#include <vector>

using asio::ip::tcp;

using single_domino = std::pair<int, int>;
using domino_vector = std::vector<single_domino>;
using dominoes_queue = std::deque<std::pair<int, int>>;

class player {
public:
  player(tcp::socket socket, std::string login, std::uint16_t pos)
      : socket_(std::move(socket)), user_login(std::move(login)), pos_(pos) {}

  bool check_active() { return socket_.is_open(); }
  void add_dominoes(single_domino &&domino) {
    user_domino.emplace_back(domino);
  }

  void send_dominoes() {
    char buffer[168] = "OK   ";
    for (const auto &e : user_domino) {
      sprintf(buffer, "%s [%d|%d]", buffer, e.first, e.second);
    }
    std::error_code ec;
    write(std::string(buffer));
  }

  void write(std::string msg) { asio::write(socket_, asio::buffer(msg)); }

  auto &recieve_dominoes(std::uint16_t i) { return user_domino[i]; }
  void delete_domino(std::uint16_t i) {
    auto it = user_domino.begin();
    std::advance(it, i);
    user_domino.erase(it);
  }
  friend class game_room;
  friend class game_server;

private:
  bool disconnect = false;
  std::string user_login;
  tcp::socket socket_;
  std::uint16_t pos_;
  std::uint16_t domino_left = 0;
  bool cant_move = false;
  domino_vector user_domino;
};

//----------------------------------------------------------------------

class game_room {
public:
  game_room(asio::io_context &io_context) : io_context_(io_context) {}
  void join(tcp::socket participant, std::string login) {
    participants_.emplace_back(
        player(std::move(participant), std::move(login), participants_.size()));
  }

  // void leave(player_ptr participant) { participants_.erase(participant); }
  void start_game() {

    std::cout << "Start game\n";
    for (auto &participant : participants_) {
      for (int i = 0; i < 7; i++) {
        participant.add_dominoes(table.get_domino());
      }
    }

    for (auto &participant : participants_) {
      participant.write("Game starts");
      asio::steady_timer t(io_context_, asio::chrono::seconds(1));
    }

    while (!game_is_over) {
      int i = 0;
      for (auto &participant : participants_) {

        i++;

        print_table(participant);
        bool end_step = false;
        while (!end_step) {
          end_step = recieve_command(participant);

          bool was_disconnected = false;
          if (participant.disconnect) {
            std::cout << "Disconnected\n";
            was_disconnected = true;
            disconnected++;
          }
          for (int time_to_live = 0; participant.disconnect; time_to_live++) {
            if (time_to_live == 53) {
              game_is_over = true;
              std::cout << "Game over\nPlayer " << participant.user_login
                        << " quit\n";
              break;
            }
            asio::steady_timer t(io_context_, asio::chrono::seconds(5));
            t.wait();
          }
          if (!participant.disconnect && was_disconnected) {
            std::cout << "Connected\n";
            participant.write("Reconnected");
            print_table(participant);
            disconnected--;
          }
        }

        if (participant.user_domino.size() == 0) {
          game_is_over = true;
          std::cout << "Game over\nPlayer " << participant.user_login
                    << " wins\n";
          break;
        }

        int n = 0;
        for (auto &participant : participants_) {
          if (participant.cant_move) {
            n++;
          }
        }
        if (n == participants_.size()) {
          game_is_over = true;
          std::cout << "Game over\nNoone wins";
          for (auto &participant : participants_) {
            participant.write("FISH\n");
          }
        }
      }
    }
    for (auto &participant : participants_) {
      if (!participant.disconnect) {
        participant.write("END\n");
      }
      participant.socket_.close();
    }
  }

  bool try_to_add(single_domino sd) {
    std::cout << sd.first << '|' << sd.second << '\n';
    if (game.empty()) {
      game.push_back(sd);
      return true;
    }

    if (sd.first == game.back().second) {
      game.push_back(sd);
      return true;

    } else if (sd.second == game.front().first) {
      game.push_front(sd);
      return true;
    }

    std::swap(sd.first, sd.second);
    if (sd.first == game.back().second) {
      game.push_back(sd);
      return true;

    } else if (sd.second == game.front().first) {
      game.push_front(sd);
      return true;

    } else {
      return false;
    }
  }

  bool recieve_command(player &participant) {

    /*bool active = check_online(participant);
    if (!active)
      return false;*/

    std::array<char, 25> buf;
    std::error_code ec;
    std::size_t len;
    for (int i = 0; i < 6; i++) {
      len = participant.socket_.read_some(asio::buffer(buf, 5), ec);
      if (!ec) {
        break;
      } else if (i == 5) {
        participant.disconnect = true;
        return false;
      } else {
        asio::steady_timer t(io_context_, asio::chrono::seconds(5));
        t.wait();
      }
    }
    std::string command{buf.data()};

    std::cout << "Recieved command: " << len << ' ' << command << std::endl;
    if (command.find("POST") != command.npos) {

      participant.socket_.read_some(asio::buffer(buf, 2));

      bool added =
          try_to_add(participant.recieve_dominoes(std::stoi(buf.data()) - 1));

      if (added) {
        participant.write(("OK   "));
        participant.cant_move = false;
        participant.delete_domino(std::stoi(buf.data()) - 1);
        return true;
      } else {
        participant.write(("ERR  "));
        return false;
      }
    } else if (command.find("GET") != command.npos) {
      if (table.size() != 0) {
        participant.add_dominoes(table.get_domino());
        participant.write("OK   ");
        return false;
      } else {
        participant.cant_move = true;
        participant.write("ERR  ");
        return true;
      }
    } else if (command.find("PRINT") != command.npos) {
      participant.send_dominoes();
      return false;
    } else if (command.find("SKIP") != command.npos) {
      participant.cant_move = true;
      return true;
    }
    return false;
  }
  void print_table(player &participant) {
    char buffer[168] = "";
    if (!game.empty()) {
      for (auto &e : game) {
        sprintf(buffer, "%s[%d|%d]", buffer, e.first, e.second);
      }
    }
    sprintf(buffer, "%s\nNow your turn", buffer);
    participant.write(buffer);
  }

  std::size_t size() const { return participants_.size(); }

  bool check_online(player &participant) {
    int time_to_wait{2};
    for (int i = 1; participant.socket_.available() != 0 && i < 55; i++) {
      if (participant.socket_.available() != 0)
        break;
      asio::steady_timer t(io_context_, asio::chrono::seconds(time_to_wait));
      t.wait();
      std::cout << participant.user_login << " not available\n";
      participant.disconnect = participant.socket_.available() != 0;
      if (i == 30) {
        time_to_wait = 5;
        participant.disconnect = true;
      } else if (i == 54) {
        participant.disconnect = true;
        return false;
      }
    }
    return true;
  }

  bool game_is_over = false;
  int disconnected = 0;

  friend class game_server;

private:
  std::vector<player> participants_;
  int n = 0;
  dominoes_queue game;
  dominoes table;

  asio::io_context &io_context_;
};

//----------------------------------------------------------------------

class game_server {
public:
  game_server(asio::io_context &io_context, const tcp::endpoint &endpoint)
      : acceptor_(io_context, endpoint), io_context_(io_context),
        room_(new game_room(io_context)) {
    do_accept();
  }
  void start_game() { room_->start_game(); }

private:
  void do_accept() {

    acceptor_.async_accept([this](std::error_code ec, tcp::socket socket) {
      if (!ec && room_->game_is_over) {
        game_room *new_room = new game_room(io_context_);
        delete room_;
        room_ = new_room;
      }

      if (!ec && room_->size() - room_->disconnected < 2) {
        std::array<char, 5> head;
        socket.read_some(asio::buffer(head, 5));
        std::array<char, 128> body;
        std::size_t len = socket.read_some(asio::buffer(body), ec);

        std::string header = head.data();
        std::string body_str = body.data();
        body_str = body_str.substr(0, len);

        if (header.find("AUTH") != header.npos) {
          std::string login = body_str.substr(0, body_str.find_first_of(':'));
          std::string password_str =
              body_str.substr(body_str.find_first_of(':') + 1, body.size());

          std::cout << password_str << std::endl;
          asio::io_context io_context_;
          tcp::socket auth_socket(io_context_);
          tcp::resolver resolver(io_context_);

          asio::connect(auth_socket, resolver.resolve("127.0.0.1", "8000"));
          auth_socket.write_some(asio::buffer("CHECK" + body_str));
          std::array<char, 128> answer;
          auth_socket.read_some(asio::buffer(answer));
          std::string answer_str(answer.data());
          auth_socket.close();

          if (answer_str.find("OK") != answer_str.npos) {

            std::cout << "Logged\n";
            if (room_->disconnected != 0) {
              std::cout << "Try to recconect\n";
              for (auto &participant : room_->participants_) {
                if (participant.disconnect && participant.user_login == login) {
                  participant.socket_ = std::move(socket);
                  participant.disconnect = false;
                }
              }
            } else {
              bool exist = false;
              for (auto &participant : room_->participants_) {
                if (participant.user_login == login)
                  exist = true;
              }
              if (exist) {
                socket.write_some(asio::buffer("Player exist\n"));
                socket.close();
              } else {
                room_->join(std::move(socket), std::move(login));
                if (room_->size() == 2) {
                  t = std::thread(&game_server::start_game, this);
                  t.detach();
                }
              }
            }
          } else {
            socket.write_some(asio::buffer("Auth failed\n"));
            socket.close();
          }
        }
      } else {
        socket.write_some(asio::buffer("Room is full\n"));
        socket.close();
      }
      do_accept();
    });
  }

  std::thread t;
  tcp::acceptor acceptor_;
  asio::io_context &io_context_;
  game_room *room_;
};

//----------------------------------------------------------------------

int main(int argc, char *argv[]) {
  try {
    if (argc < 2) {
      std::cerr << "Usage: game_server <port> [<port> ...]\n";
      return 1;
    }

    asio::io_context io_context;

    std::list<game_server> servers;
    for (int i = 1; i < argc; ++i) {
      tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[i]));
      servers.emplace_back(io_context, endpoint);
    }

    io_context.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
