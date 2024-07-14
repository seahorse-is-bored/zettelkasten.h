#ifndef ZETTELKASTEN
#define ZETTELKASTEN

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
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

  uint64_t createNote(uint64_t templateId, std::vector<std::string> flds, uint64_t deckId) {
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
        c.revHistory = {}; 
        n.cards.push_back(c);
        it++;
    }
    noteStack[n.noteId] = n;
    it = 0;
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

  void createDeck(std::string deckName, uint64_t did = 0) {
    deck d;
    d.name = deckName;
    if (getDeckByName(deckName) != 0) {
      return;
    }
    d.deckId = generateIds('d');
    if(did !=0 & !boxes.count(did)) {
      d.deckId = did;
    }
    char ch = 31;
    std::string replacement(1, ch);
    std::string str = findAndReplaceAll(deckName, "::", replacement);
    std::vector<std::string> parents = stringToVector<std::string>(str);
    /*auto it = 0;*/
    for (const std::string par : parents) {

      std::string url = vectorToString(parseVectorUpToString(parents, par));
      uint64_t par1 = getDeckByName(url);
      if (0 == par1) {
        createDeck(findAndReplaceAll(url, replacement, "::"));
        /*it++;*/
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

  std::string printCardFromId(uint64_t id, const char side) {
    card c = *allCards[id];  
    return displayCard(c, side);
  }

  void loadHistory(reviewHistory &h, uint64_t cardId) {
    card c = *allCards[cardId];
    c.revHistory.push_back(h);
  }
  
  

  void addReview(uint64_t cardId, uint64_t rating) {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    if (allCards.find(cardId)==allCards.end()) {
      std::cerr << "card not found" << "\n";
      return;
    }
    card* c = allCards[cardId];
    reviewHistory rh;
    if(c->revHistory.empty()) {
      rh.timeBetween = 0;
      rh.rating =  rating;
      rh.timeStamp = 0;
    }
    else {
      reviewHistory rh_prev;
      rh_prev = c->revHistory.back();
      rh.timeBetween = now_ms - rh_prev.timeStamp;
      rh.rating = rating;
      rh.timeStamp =  now_ms;
    }
    c->revHistory.push_back(rh);
  }


  int createDatabase(const std::string &dbName) {
    sqlite3 *shelf;
    int rc;
    char *errMsg;
    const char* tempName = "kasten.kasten1";
    rc = sqlite3_open(tempName, &shelf);
    if (rc) {
      std::cout << sqlite3_errmsg(shelf);
      return 1;
    }
    std::string sql = "CREATE TABLE cardPile(\n"
      "cardId INTEGER,\n"
      "templateId INTEGER,\n"
      "noteId INTEGER,\n"
      "noteVar INTEGER,\n"
      "deckId INTEGER, \n"
      "timeBetween TEXT,\n"
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
      return 1;
    }
    for (const auto& [cid, c_ptr] : allCards) {
      card c = *c_ptr;
      std::string timeBetweenStr = "";
      std::string ratHistStr = "";
      std::string timesHistStr = "";
      if (!c.revHistory.empty()) {
          for(const auto rev : c.revHistory) {
            char sep = 31;
            timeBetweenStr = timeBetweenStr.empty() ? std::to_string(rev.timeBetween) : timeBetweenStr + sep + std::to_string(rev.timeBetween);
            ratHistStr = ratHistStr.empty() ? std::to_string(rev.rating) : ratHistStr + sep + std::to_string(rev.rating);
            timesHistStr =timesHistStr.empty() ? std::to_string(rev.timeStamp) : timesHistStr + sep + std::to_string(rev.timeStamp);
          }
      }
      sql = "INSERT INTO cardPile(cardId, templateId, noteId, noteVar, deckId, timeBetween, timeHistory, ratingHistory) VALUES ("
        + std::to_string(c.cardId) + ", "
        + std::to_string(c.templateIdNoteVar[0]) + ", "
        + std::to_string(c.noteId) + ", "
        + std::to_string(c.templateIdNoteVar[1]) + ", "
        + std::to_string(c.deckId) + ", '"
        + timeBetweenStr + "', '" + timesHistStr + "', '" + ratHistStr + "');";
      if (sqlite3_exec(shelf, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return 1;
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
            return 1;
        }
    }
    for(const auto& [did, dk] : boxes) {
      sql = "INSERT INTO boxes(deckId, deckName) VALUES ("
        + std::to_string(did) + ", '"
        + dk.name + "');";
      if (sqlite3_exec(shelf, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cerr << "SQL error card: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            return 1;
      }
    }
    for(const auto& [tid, tplt] : templates) {
      sql = "INSERT INTO templateCollection(templateId, templateName, frontLayout, reverseLayout, fldNames) VALUES ("
        + std::to_string(tid) + ", '"
        + tplt.name + "', '" 
        + vectorToString(tplt.frontLayout) + "', '"
        + vectorToString(tplt.reverseLayout) + "', '"
        + vectorToString(tplt.fldNames) + "');";
      if (sqlite3_exec(shelf, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cerr << "SQL error card: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            return 1;
      }
    }
    sqlite3_close(shelf);

    const char* newName = dbName.c_str();
    if(std::rename(tempName, newName) != 0) {
      std::perror("Error renaming new database");
    }
    return 0;
  }
  int loadFromDatabase(const std::string &dbName) {
    sqlite3 *shelf;
    int rc;
    char *errMsg = nullptr;

    rc = sqlite3_open(dbName.c_str(), &shelf);
    if (rc != SQLITE_OK) {
        std::cerr << "cannot open database: " << sqlite3_errmsg(shelf) << std::endl;
        sqlite3_close(shelf);
        return 1;
    }

    std::string sql = "SELECT * FROM cardPile;";
    sqlite3_stmt *stmt;
    std::unordered_map<uint64_t, card> cStack;

    if (sqlite3_prepare_v2(shelf, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            card c;
            uint64_t cId = sqlite3_column_int64(stmt, 0);
            uint64_t tId = sqlite3_column_int64(stmt, 1);
            uint64_t nId = sqlite3_column_int64(stmt, 2);
            uint64_t nVar = sqlite3_column_int64(stmt, 3);
            uint64_t dId = sqlite3_column_int64(stmt, 4);
            const char *timeBetweenCStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            const char *timesHistCStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            const char *ratHistCStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));

            std::string timeBetweenStr = timeBetweenCStr ? timeBetweenCStr : "";
            std::string timesHistStr = timesHistCStr ? timesHistCStr : "";
            std::string ratHistStr = ratHistCStr ? ratHistCStr : "";

            c.cardId = cId;
            c.noteId = nId;
            c.templateIdNoteVar = {tId, nVar};
            c.deckId = dId;

            if (!timeBetweenStr.empty()) {
                std::cout << timeBetweenStr << "\t" << timesHistStr << "\t" << ratHistStr <<"\n";
                std::vector<uint64_t> timeBetween = stringToVector<uint64_t>(timeBetweenStr);
                std::vector<uint64_t> timeHistory = stringToVector<uint64_t>(timesHistStr);
                std::vector<uint64_t> ratingHistory = stringToVector<uint64_t>(ratHistStr);
                size_t i = 0;
                for (const auto &rh : timeHistory) {
                    reviewHistory h;
                    h.timeBetween = timeBetween[i];
                    h.timeStamp =  timeHistory[i];
                    h.rating = ratingHistory[i];
                    c.revHistory.push_back(h);
                    i++;
                }
            }
           

            cStack[c.cardId] = c;
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Failed to prepare statement for cardPile: " << sqlite3_errmsg(shelf) << std::endl;
        sqlite3_close(shelf);
        return 1;
    }

    sql = "SELECT * FROM notes;";
    if (sqlite3_prepare_v2(shelf, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            note n;
            uint64_t nId = sqlite3_column_int64(stmt, 0);
            const char *fldsCStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

            std::string flds = fldsCStr ? fldsCStr : "";

            n.noteId = nId;
            n.flds = stringToVector<std::string>(flds);

            for (auto &[cid, c] : cStack) {
                if (c.noteId == nId) {
                  n.cards.push_back(c);
                }
            }
            noteStack[n.noteId] = n;

            size_t it = 0;
            for (auto &c : noteStack[n.noteId].cards) {
                allCards[c.cardId] = &noteStack[n.noteId].cards[it];
                ++it;
            }
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Failed to prepare statement for notes: " << sqlite3_errmsg(shelf) << std::endl;
        sqlite3_close(shelf);
        return 1;
    }

    sql = "SELECT * FROM boxes;";
    if (sqlite3_prepare_v2(shelf, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            uint64_t dId = sqlite3_column_int64(stmt, 0);
            const char *deckNameCStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::string deckName = deckNameCStr ? deckNameCStr : "";

            createDeck(deckName, dId);
            deck &d = boxes[dId];

            for (const auto &[cid, c] : cStack) {
                if (c.deckId == dId) {
                    d.cardIds.push_back(cid);
                }
            }
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Failed to prepare statement for boxes: " << sqlite3_errmsg(shelf) << std::endl;
        sqlite3_close(shelf);
        return 1;
    }

    sql = "SELECT * FROM templateCollection;";
    if (sqlite3_prepare_v2(shelf, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cardTemplate ct;
            uint64_t ctId = sqlite3_column_int64(stmt, 0);
            const char *nameCStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char *frontCStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char *backCStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char *stylingCStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

            std::string front = frontCStr ? frontCStr : "";
            std::string back = backCStr ? backCStr : "";
            std::string fldsStr = stylingCStr ? stylingCStr : "";
            
            ct.id = ctId;
            ct.name = nameCStr ? nameCStr:"";
            ct.frontLayout = stringToVector<std::string>(front);
            ct.reverseLayout = stringToVector<std::string>(back);
            ct.fldNames = stringToVector<std::string>(fldsStr);

            templates[ctId] = ct;
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Failed to prepare statement for templateCollection: " << sqlite3_errmsg(shelf) << std::endl;
        sqlite3_close(shelf);
        return 1;
    }

    sqlite3_close(shelf);
    return 0;
}

};

#endif
