#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>

#include "room.h"


class ServerCtx {
 public:
  ServerCtx() {}

  std::optional<Room> createRoom(std::string roomId) {
    if (rooms_.contains(roomId))
    {
      return {};
    }
    Room room(roomId);
    auto roomPair = rooms_.insert({roomId, room});
    if (!roomPair.second)
    {
      return {};
    }
    return roomPair.first->second;
  }

  std::optional<Room> getRoom(std::string roomId) {
    std::variant<int, Room> result;
    auto roomIter = rooms_.find(roomId);
    if(roomIter == rooms_.end()) {
      return {};
    }
    return roomIter->second;
  }

 private:
   std::unordered_map<std::string, Room> rooms_;
};