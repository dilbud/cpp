
#ifndef USER_H
#define USER_H

#include <memory>
#include <string>


class User {
 public:
  User(std::string name) 
   : name_(name) {}

 private:
  std::string name_;
};

#endif // USER_H