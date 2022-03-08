//
// Created by misc1 on 3/3/2022.
//

#include "VerticalScrabbleVectorizer.h"
#include <sstream>

VerticalScrabbleVectorizer::VerticalScrabbleVectorizer() {
    ifstream englishWords;
    englishWords.open("../Data/scrabble_word_list.txt");
    if(!englishWords.is_open())
        throw invalid_argument("could not open ../Data/scrabble_word_list.txt");

    string curWord;
    while(englishWords.good()){
        getline(englishWords, curWord);
        scrabbleWordSet.emplace(LString(curWord));
    }
    englishWords.close();
}

VerticalScrabbleVectorizer::VerticalScrabbleVectorizer(const string &passed) {
    hand = passed;

    ifstream englishWords;
    englishWords.open("../Data/scrabble_word_list.txt");
    if(!englishWords.is_open())
        throw invalid_argument("could not open ../Data/scrabble_word_list.txt");

    string curWord;
    while(englishWords.good()){
        getline(englishWords, curWord);
        scrabbleWordSet.emplace(LString(curWord));
    }
    englishWords.close();
}

void VerticalScrabbleVectorizer::build_board(const string& filePath) {
    ifstream boardFile;
    boardFile.open(filePath);
    if(!boardFile.is_open())
        throw invalid_argument("could not open " + filePath);

    string row;
    int rowCount = 0;
    while (boardFile.good()){
        getline(boardFile, row);
        string cell;
        stringstream strStr(row);
        int cellCount = 0;
        LString rowVect;
        while (getline(strStr, cell, ',') && cellCount < 15){
            if(!cell.empty() && isalpha(cell[0])) {
                rowVect.push_back(Letter(cell[0], cellCount, rowCount, 1));
            }
            else {
                rowVect.push_back(Letter(' ', cellCount, rowCount, 1));
            }
            cellCount++;
        }
        board.push_back(rowVect);
        rowCount++;
    }
    boardFile.close();

    vector<LString> boardCpy;
    for (int i = 14; i >= 0; i--) {  //i = x
        LString column;
        for (int j = 0; j < 15; j++) {  //j = y
            column += Letter(board[j][i].LData, j, 14 - i, 1);
        }
        boardCpy.push_back(column);
    }
    board = boardCpy;

    for (int i = 0; i < 15; ++i) {
        for (int j = 0; j < 15; ++j) {
            if(board[i][j] != ' ')
                perkBoard[i][j] = ' ';
        }
    }
}

void VerticalScrabbleVectorizer::print_formatted_board() const{
    for (int i = 0; i < 15; ++i) {
        for (int j = 14; j >= 0; --j) {
            cout << board[j].read_at(i).LData;
        }
        cout << endl;
    }
}

string VerticalScrabbleVectorizer::to_string() const {
    string buffer = "Hand: " + hand + "\n";
    buffer += "Best Vertical Word: " + bestWord.to_string() + " - " + ::to_string(bestWord.get_letter_points() +
                                                                                   perpendicular_points(bestWord));
    buffer += "\n\tPostion X: " + ::to_string(bestX);
    buffer += "\n\tPostion Y: " + ::to_string(bestY);
    buffer += "\n\tVertical";

    return buffer;
}

LString VerticalScrabbleVectorizer::update_best_word(){
    int rowSubscript = 0;
    bestWord.clear();
    for (auto & wordSet : answerSets) {
        for (const auto& word: wordSet) {
            int wordPoints = points_of_word(word) + points_of_word(word);
            int bestWordPoints = points_of_word(bestWord) + points_of_word(bestWord);

            if (wordPoints > bestWordPoints) {
                bestWord = word;
                //x = y, y = 14 - x
                bestX = (14 - rowSubscript) + 1;
                bestY = (bestWord[0].x) + 1;
            } else if (wordPoints == bestWordPoints) {
                if (word.length() < bestWord.length() || bestWord.is_empty()) {
                    bestWord = word;
                    //x = y, y = 14 - x
                    bestX = (14 - rowSubscript) + 1;
                    bestY = (bestWord[0].x) + 1;
                }
            }
        }
        rowSubscript++;
    }

    return bestWord;
}

void VerticalScrabbleVectorizer::validate_words() {
    for (auto & wordSet : answerSets) {
        for (auto& word: wordSet) {
            vector<LString> boardCpy = return_raw_board_with(word);

            for (int i = 0; i < 15; i++) {
                LString row;
                LString column;
                for (int j = 0; j < 15; ++j) {
                    row += boardCpy[i].read_at(j);
                    column += boardCpy[14 - j].read_at(i);
                }

                vector<LString> colShards = column.break_into_frags();

                for (const auto& shard : colShards) {
                    if(shard.length() > 1 && scrabbleWordSet.find(shard) == scrabbleWordSet.end()) {
                        word.clear();
                    }
                }

                vector<LString> rowShards = row.break_into_frags();

                for (const auto& shard : rowShards) {
                    if(shard.length() > 1 && scrabbleWordSet.find(shard) == scrabbleWordSet.end()) {
                        word.clear();
                    }
                }
            }
        }
    }
}

void VerticalScrabbleVectorizer::set_board(vector<LString> passed) {   //assumes that passed is formatted as a proper board
    vector<LString> boardCpy;
    for (int i = 14; i >= 0; i--) {  //i = x
        LString column;
        for (int j = 0; j < 15; j++) {  //j = y
            column += Letter(passed[j][i].LData, j, 14 - i, 1);
        }
        boardCpy.push_back(column);
    }
    board = boardCpy;

    for (int i = 0; i < 15; ++i) {
        for (int j = 0; j < 15; ++j) {
            if(board[i][j] != ' ')
                perkBoard[i][j] = ' ';
        }
    }
}

void VerticalScrabbleVectorizer::validate_board() const{
    if(scrabbleWordSet.empty())
        throw invalid_argument("Error in ScrabbleVectorizer::validate_board() | unordered_map<LString> scrabbleWordSet is empty.");

    for (int i = 0; i < 15; ++i) {
        LString column;
        for (int j = 0; j < 15; ++j) {
            column += board[i].read_at(j);
        }

        LString row;
        for (int j = 14; j >= 0; --j) {
            row += board[j].read_at(i);
        }

        vector<LString> colShards = column.break_into_frags();

        for (const auto& shard : colShards) {
            if(shard.length() > 1 && scrabbleWordSet.find(shard) == scrabbleWordSet.end())
                throw invalid_argument("Invalid vertical Word in Data/Board.csv | " + shard.to_string());
        }

        vector<LString> rowShards = row.break_into_frags();

        for (const auto& shard : rowShards) {
            if(shard.length() > 1 && scrabbleWordSet.find(shard) == scrabbleWordSet.end())
                throw invalid_argument("Invalid horizontal Word in Data/Board.csv | " + shard.to_string());
        }
    }
}

vector<string> VerticalScrabbleVectorizer::return_formatted_perkBoard() const {
    vector<string> toReturn;
    for (int i = 0; i < 15; ++i) {
        string column;
        for (int j = 14; j >= 0; --j) {
            column += perkBoard[j][i];
        }
        toReturn.push_back(column);
    }
    return toReturn;
}

vector<LString> VerticalScrabbleVectorizer::return_formatted_board_with(const LString &toPrint) const {
    vector<LString> boardCpy = board;
    int toPrintX = toPrint.read_at(0).y;
    int toPrintY = 14 - toPrint.read_at(0).x;
    for (int i = toPrintY; i >= toPrintY - toPrint.length(); i--) {
        if(boardCpy[i][toPrintX] == ' ')
            boardCpy[i][toPrintX] = Letter(toPrint.read_at(toPrintY - i).LData, i, toPrintX, -2);
        else
            boardCpy[i][toPrintX].flag = 1;
    }

    vector<LString> boardCpy2;
    for (int i = 0; i < 15; i++) {
        LString column;
        for (int j = 14; j >= 0; j--) {
            column += boardCpy[j].read_at(i);
        }
        boardCpy2.push_back(column);
    }

    return boardCpy2;
}

vector<string> VerticalScrabbleVectorizer::return_formatted_char_board() const {
    vector<string> boardCpy;
    for (int i = 0; i < 15; ++i) {
        string column;
        for (int j = 14; j >= 0; --j) {
            column += board[j].read_at(i).LData;
        }
        boardCpy.push_back(column);
    }
    return boardCpy;
}

vector<vector<LString>> VerticalScrabbleVectorizer::return_formatted_answerSets() const {
    vector<vector<LString>> toReturn;
    for (int i = 0; i < 15; ++i) {
        toReturn.push_back(answerSets[14 - i]);
    }
    return toReturn;
}

vector<LString> VerticalScrabbleVectorizer::return_formatted_board() const {
    vector<LString> boardCpy;
    for (int i = 0; i < 15; ++i) {
        LString column;
        for (int j = 14; j >= 0; --j) {
            column += board[j].read_at(i);
        }
        boardCpy.push_back(column);
    }
    return boardCpy;
}
