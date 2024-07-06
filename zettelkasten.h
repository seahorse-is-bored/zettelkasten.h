#ifndef ZETTELKASTEN
#define ZETTELKASTEN

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include <sqlite3.h>

struct reviewHistory {
  uint64_t timeBetween;
  uint64_t rating;
  uint64_t timeStamp;
};

struct card {
  uint64_t cardId;
  uint64_t noteId;
  uint64_t deckId;
  std::vector<uint64_t> templateIdNoteVar;
  std::vector<reviewHistory> revHistory;
};

struct note {
  std::vector<card> cards;
  std::vector<std::string> flds;
  uint64_t noteId;
};

struct deck {
  std::string name;
  std::vector<uint64_t> cardIds;
  std::vector<deck *> parents;
  uint64_t deckId;
};

struct cardTemplate {
  std::string name;
  uint64_t id;
  std::vector<std::string> frontLayout;
  std::vector<std::string> reverseLayout;
  std::vector<std::string> fldNames;
};

class Zettelkasten {
private:
  // Private functions
  uint64_t randomUInt64() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis(
        0, std::numeric_limits<uint64_t>::max());
    return dis(gen);
  }
  template <typename MapType>
  bool checkStacks(const MapType &map, uint64_t key) {
    auto filter = [&key](const auto &pair) { return pair.first == key; };
    auto it = std::find_if(map.begin(), map.end(), filter);
    return (it != map.end());
  }

  // General ID generation function
  template <typename MapType> uint64_t _id_machine(const MapType &map) {
    uint64_t key = randomUInt64();
    while (checkStacks(map, key)) {
      key = randomUInt64();
    }
    return key;
  }
  template <typename T> std::string vectorToString(const std::vector<T> &vec) {
    char separator = 31; // Unicode unit separator
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
      if (i != 0) {
        oss << separator;
      }
      oss << vec[i];
    }
    return oss.str();
  }
  template <> std::string vectorToString(const std::vector<char> &vec) {
    char separator = 31; // Unicode unit separator
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
      if (i != 0) {
        oss << separator;
      }
      oss << vec[i];
    }
    return oss.str();
  }
  template <typename T>
  std::vector<T> stringToVector(const std::string &str, char separator = 31) {
    std::vector<T> result;
    std::istringstream iss(str);
    std::string token;

    while (std::getline(iss, token, separator)) {
      if constexpr (std::is_same_v<T, std::string>) {
        result.push_back(token);
      } else if constexpr (std::is_same_v<T, char>) {
        result.push_back(token[0]);
      } else {
        std::istringstream tokenStream(token);
        T value;
        tokenStream >> value;
        result.push_back(value);
      }
    }

    return result;
  }

  std::vector<std::string>
  parseVectorUpToString(const std::vector<std::string> &vec,
                        const std::string &target) {
    std::vector<std::string> result;

    bool found = false;
    for (const auto &str : vec) {
      if (str == target) {
        found = true;
        break;
      }
      result.push_back(str);
    }

    if (!found) {
      result.clear();
    }

    return result;
  }

public:
  // Lists
  std::unordered_map<uint64_t, card *>
      allCards; // This is now good because we can go directly from a cardId to
                // a card without duplicating the card
  std::unordered_map<uint64_t, note> noteStack;
  std::unordered_map<uint64_t, deck> boxes;
  std::unordered_map<uint64_t, cardTemplate> templates;
  std::unordered_map<uint64_t, card *>
      dueStack; // Format <next due to millisecond, card pointer>

  // Functions

  uint64_t generateIds(char stackType) {
    switch (stackType) {
    case 'c':
      return _id_machine(allCards);
    case 'n':
      return _id_machine(noteStack);
    case 'd':
      return _id_machine(boxes);
    case 't':
      return _id_machine(templates);
    default:
      return 0; // or handle error
    }
  }

  std::string findAndReplaceAll(std::string str, const std::string &from,
                                const std::string &to) { // to find, from, to
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
      str.replace(start_pos, from.length(), to);
      start_pos += to.length();
    }
    return str;
  }

  std::string displayCard(card &c, const char side) {
    cardTemplate ct = templates[c.templateIdNoteVar[0]];
    note n = noteStack[c.noteId];
    std::string cardFace;
    size_t it = 0;
    switch (side) {
    case 'f':
      cardFace = ct.frontLayout[c.templateIdNoteVar[1]];
      break;
    case 'r':
      cardFace = ct.reverseLayout[c.templateIdNoteVar[1]];
      break;
    default:
      return "";
    }
    for (const std::string &fld : ct.fldNames) {
      std::string _mod = "{{" + fld + "}}";
      // std::cout << _mod << ": " << ": field: " << n.flds[it] << "\n";
      cardFace = findAndReplaceAll(cardFace, _mod, n.flds[it]);
      it++;
    }
    return cardFace;
  }

  uint64_t createNote(uint64_t templateId, std::vector<std::string> flds,
                      uint64_t deckId) {
    note n;
    cardTemplate t = templates[templateId];
    if (flds.size() != t.fldNames.size()) {
      std::cerr << "unequal card size";
      return 0;
    }
    n.flds = flds;
    n.noteId = generateIds('n');
    size_t it = 0;
    for (const std::string &layout : t.frontLayout) {
      card c;
      c.cardId = generateIds('c');
      c.noteId = n.noteId;
      c.templateIdNoteVar = {templateId, it};
      c.deckId = deckId;
      n.cards.push_back(c);
      /*allCards[c.cardId] = &n.cards[it];*/
      it++;
    }
    noteStack[n.noteId] = n;
    it=0;
    for (const auto c : noteStack[n.noteId].cards) {
      allCards[c.cardId] = &noteStack[n.noteId].cards[it];
      it++;
    }
    return n.noteId;
  }

  void appendTemplate(cardTemplate &templateNoId) {
    templateNoId.id = generateIds('t');
    templates[templateNoId.id] = templateNoId;
  }

  void createDeck(std::string deckName) {
    deck d;
    d.name = deckName;
    if (getDeckByName(deckName) != 0) {
      return;
    }
    d.deckId = generateIds('d');
    char ch = 31;
    std::string replacement(1, ch);
    std::string str = findAndReplaceAll(deckName, "::", replacement);
    std::vector<std::string> parents = stringToVector<std::string>(str);
    auto it = 0;
    for (const std::string par : parents) {

      std::string url = vectorToString(parseVectorUpToString(parents, par));
      uint64_t par1 = getDeckByName(url);
      if (0 == par1) {
        createDeck(findAndReplaceAll(url, replacement, "::"));
        it++;
      } else {
        d.parents.push_back(&boxes[par1]);
      }
    }
    boxes[d.deckId] = d;
  }

  uint64_t getDeckByName(const std::string &search) {
    // Create a temporary deck with only the name field set
    for (auto it = boxes.begin(); it != boxes.end(); ++it) {
      if (it->second.name == search) {
        return it->first;
      }
    }
    return 0;
  }

  void loadHistory(reviewHistory &h, uint64_t cardId) {
    card c = *allCards[cardId];
    c.revHistory.push_back(h);
  }
  int createDatabase(const std::string &dbName) {
    sqlite3 *shelf;
    int rc;
    char *errMsg;
    rc = sqlite3_open("kasten.kasten1", &shelf);
    if (rc) {
      std::cout << sqlite3_errmsg(shelf);
      return 0;
    }
    std::string sql = "CREATE TABLE cardPile(\n"
      "cardId INTEGER,\n"
      "templateId INTEGER,\n"
      "noteId INTEGER,\n"
      "noteVar INTEGER,\n"
      "deckId INTEGER, \n"
      "revHistory TEXT,\n"
      "timeHistory TEXT, \n"
      "ratingHistory TEXT);\n"
      "CREATE TABLE notes(\n"
      "noteId INTEGER,\n"
      "flds TEXT);\n"
      "CREATE TABLE boxes(\n"
      "deckId INTEGER,\n"
      "deckName TEXT);\n" // Create parents from deck name like Deck##Subdeck
      "CREATE TABLE  templateCollection(\n"
      "templateId INTEGER,\n"
      "templateName TEXT, \n"
      "frontLayout TEXT,\n"
      "reverseLayout TEXT,\n"
      "fldNames TEXT);";
    if(sqlite3_exec(shelf,sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
      std::cerr << "SQL error: " << errMsg << std::endl;
      sqlite3_free(errMsg);
      return 0;
    }
    for (const auto& [cid, c_ptr] : allCards) {
      card c = *c_ptr;
      /*std::cout << << std::endl;*/
      std::string revHistStr = "";
      std::string ratHistStr = "";
      std::string timesHistStr = "";
      if (sizeof(c.revHistory) > 24) { // 24 is the size of an empty reviewHistory vector
          for(const auto rev : c.revHistory) {
            char sep = 31;
            revHistStr = revHistStr + sep + std::to_string(rev.timeBetween);
            ratHistStr = ratHistStr + sep + std::to_string(rev.rating);
            timesHistStr = timesHistStr + sep + std::to_string(rev.timeStamp);
          }
      }
      sql = "INSERT INTO cardPile(cardId, templateId, noteId, noteVar, deckId, revHistory, timeHistory, ratingHistory) VALUES ("
        + std::to_string(c.cardId) + ", "
        + std::to_string(c.templateIdNoteVar[0]) + ", "
        + std::to_string(c.noteId) + ", "
        + std::to_string(c.templateIdNoteVar[1]) + ", "
        + std::to_string(c.deckId) + ", '"
        + revHistStr + "', '" + timesHistStr + "', '" + ratHistStr + "');";
      std::cout << sql; 
      if (sqlite3_exec(shelf, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return 0;
      } 
    }
    for(const auto& [nid, nt] : noteStack) {
      std::string flds = vectorToString(nt.flds);
      sql = "INSERT INTO notes(noteId, flds) VALUES ("
        + std::to_string(nid) + ", '"
        + flds + "');";
        if (sqlite3_exec(shelf, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cerr << "SQL error card: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            return 0;
        }
    }
    for(const auto& [did, dk] : boxes) {
      sql = "INSERT INTO boxes(deckId, deckName) VALUES ("
        + std::to_string(did) + ", '"
        + dk.name + "');";
      std::cout << sql;
      if (sqlite3_exec(shelf, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cerr << "SQL error card: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            return 0;
      }
    }
    sqlite3_close(shelf);
    return 1;
  }

  

};

#endif
