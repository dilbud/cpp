#include <stdio.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

struct PersonInfo {
    std::string name;
    std::vector<std::string> phones;
};

int main(int argc, char **argv) {
    std::string line;  // will hold a line and word from input, respectively
    std::string word;
    std::vector<PersonInfo> people;  // will hold all the records from the input
    // read the input a line at a time until cin hits end-of-file (or another error)

    while (std::getline(std::cin, line)) {
        PersonInfo info;                  // create an object to hold this record’s data
        std::istringstream record(line);       // bind record to the line we just read
        record >> info.name;              // read the name
        while (record >> word)            // read the phone numbers
            info.phones.push_back(word);  // and store them
        people.push_back(info);           // append this record to people
    }
}
