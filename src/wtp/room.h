#ifndef ROOM_H
#define ROOM_H

#include <string>
#include <list>
#include <variant>
#include <atomic>

#include "user.h"

class Room {
 public:
  Room(std::string const& roomId) : roomId_(roomId) {}


  std::variant<int, User> addUser(std::string userId) {
   if(!(hold < MAX_HOLD)) {
      return 1;
   }
   ++hold;
   return 0;
  }

  int removeUser(std::string userId) {
   return 0;
  }

  std::variant<int, User> getUser() {
   return 0;
  }
 private:
  std::string roomId_;

  std::list<User> users_;
  int hold = -1;
  const int MAX_HOLD = 8;
};

#endif // ROOM_H