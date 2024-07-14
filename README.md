# zettelkasten.h Documentation
A simple (incomplete) C++ library to allow for the generation, retrieval, saving of flashcards.

## Overview

The `Zettelkasten` class is designed for managing a personal knowledge database using a flashcard system. It provides functionality to handle cards, notes, decks, and templates, as well as to save and load data from an SQLite database.

## Compiling

Requires: `clang, lsqlite3, cpp>17`
```bash
clang++ myImplementation.cpp -std=c++17 -lsqlite3
```
## Data Structures

### `reviewHistory`

```cpp
struct reviewHistory {
  uint64_t timeBetween;
  uint64_t rating;
  uint64_t timeStamp;
};
``` 

**Description**: Stores the review history event for a card, including the time between reviews, rating, and timestamp.

### `card`

```cpp
struct card {
  uint64_t cardId;
  uint64_t noteId;
  uint64_t deckId;
  std::vector<uint64_t> templateIdNoteVar;
  std::vector<reviewHistory> revHistory;
}; 
```
**Description**: Represents a flashcard with an ID, associated note, deck, template details, and review history.

### `note`

```cpp
struct note {
  std::vector<card> cards;
  std::vector<std::string> flds;
  uint64_t noteId;
}; 
```

**Description**: Represents a note containing multiple cards and fields. Each note has a unique ID.

### `deck`


```cpp
struct deck {
  std::string name;
  std::vector<uint64_t> cardIds;
  std::vector<deck *> parents;
  uint64_t deckId;
};
```

**Description**: Represents a deck of cards with a name, a list of card IDs, parent decks, and a unique deck ID.

### `cardTemplate`


```cpp
struct cardTemplate {
  std::string name;
  uint64_t id;
  std::vector<std::string> frontLayout;
  std::vector<std::string> reverseLayout;
  std::vector<std::string> fldNames;
};
```

**Description**: Defines the layout and fields for a card template. Includes front and reverse layouts and field names.

## Public Functions

### `uint64_t generateIds(char stackType)`

**Description**: Generates a unique ID for the specified stack type.

**Parameters**:

-   `stackType`: Character indicating the type of ID to generate (`'c'`  for card,  `'n'`  for note,  `'d'`  for deck,  `'t'`  for template).

**Returns**: A unique ID of type  `uint64_t`.

**Example**:


```cpp
uint64_t cardId = zettelkasten.generateIds('c');
``` 

### `std::string findAndReplaceAll(std::string str, const std::string &from, const std::string &to)`

**Description**: Replaces all occurrences of a substring within a string. This is used for processing strings when going directly from note->card and displaying text without using the ```displayCard``` or ```printCardFromId```

**Parameters**:

-   `str`: The original string.
-   `from`: The substring to be replaced.
-   `to`: The replacement substring.

**Returns**: The modified string.


**Example**:

```cpp
std::string result = zettelkasten.findAndReplaceAll("Hello, world!", "world", "there");
```


### `std::string displayCard(card &c, const char side)`

**Description**: Displays the card with the specified side.

**Parameters**:

-   `c`: The  `card`  object.
-   `side`: Character indicating the side of the card (`'f'`  for front,  `'r'`  for reverse).

**Returns**: The formatted string of the card's face.

**Example**:


```cpp
std::string frontFace = zettelkasten.displayCard(myCard, 'f');
```

### `uint64_t createNote(uint64_t templateId, std::vector<std::string> flds, uint64_t deckId)`

**Description**: Creates a new note with the given template and fields, and adds it to the specified deck.

**Parameters**:

-   `templateId`: ID of the card template.
-   `flds`: Vector of field values for the note.
-   `deckId`: ID of the deck to which the note should be added.

**Returns**: The unique ID of the newly created note.

**Example**:


```cpp
uint64_t newNoteId = zettelkasten.createNote(templateId, {"Field1", "Field2"}, deckId);
```

### `void appendTemplate(cardTemplate &templateNoId)`

**Description**: Adds a new card template to the collection.

**Parameters**:

-   `templateNoId`: The  `cardTemplate`  object to be added. The ID will be generated within the function.

**Example**:


```cpp
cardTemplate newTemplate;
newTemplate.name = "Basic";
newTemplate.frontLayout = {"Front"};
newTemplate.reverseLayout = {"Back"};
zettelkasten.appendTemplate(newTemplate);
```

### `void createDeck(std::string deckName, uint64_t did = 0)`

**Description**: Creates a new deck with the given name. Optionally, a specific deck ID can be provided.

**Parameters**:

-   `deckName`: The name of the deck.
-   `did`: Optional specific deck ID.

**Example**:

```cpp
zettelkasten.createDeck("Maths::Algebra");
```

### `uint64_t getDeckByName(const std::string &search)`

**Description**: Retrieves the ID of a deck by its name.

**Parameters**:

-   `search`: The name of the deck to search for.

**Returns**: The ID of the deck, or 0 if not found.

**Example**:


```cpp
uint64_t deckId = zettelkasten.getDeckByName("Maths::Algebra");
```

### `std::string printCardFromId(uint64_t id, const char side)`

**Description**: Prints the card's face from its ID.

**Parameters**:

-   `id`: The ID of the card.
-   `side`: Character indicating which side of the card to print (`'f'`  for front,  `'r'`  for reverse).

**Returns**: The formatted card face.

**Example**:


```cpp
std::string cardFace = zettelkasten.printCardFromId(cardId, 'f');
```

### `void loadHistory(reviewHistory &h, uint64_t cardId)`

**Description**: Loads review history into the specified card.

**Parameters**:

-   `h`: The  `reviewHistory`  object to be added.
-   `cardId`: ID of the card.

**Example**:


```cpp
reviewHistory rh = {10, 5, 1625237489};
zettelkasten.loadHistory(rh, cardId);
```

### `int createDatabase(const std::string &dbName)`

**Description**: Creates an SQLite database file with the provided name and initializes it with the current data.

**Parameters**:

-   `dbName`: The name of the SQLite database file.

**Returns**: 0 on success, or 1 on failure.

**Example**:

```cpp
int result = zettelkasten.createDatabase("myDatabase.db");
```

### `int loadFromDatabase(const std::string &dbName)`

**Description**: Loads data from an existing SQLite database file.

**Parameters**:

-   `dbName`: The name of the SQLite database file.

**Returns**: 0 on success, or 1 on failure.

**Example**:
```cpp
int result = zettelkasten.loadFromDatabase("myDatabase.db");
```

### `void addHistory(uint64_t cardId, uint64_t rating)`

**Description**: adds a review history element at the current time using milliseconds in the UNIX epoch to the card at a given cardId. If the cardId does not exist it returns early and prints card not found.

**Parameters**:
-   `cardId`: the Id of the card to be found from allCards
-   `rating`: the difficulty rating of a card, this functionality is for implementation of SRS in the future

**Example**:
```cpp
addHistory(cardId, 3);
```







