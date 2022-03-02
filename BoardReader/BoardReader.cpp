//
// Created by misc1 on 2/26/2022.
//

#include "BoardReader.h"
#include <sstream>

BoardReader::BoardReader() {
    bestX = bestY = 8;
    readerType = UNDEFINED;

    ifstream englishWords;
    englishWords.open("../data/englishWords.txt");
    if(!englishWords.is_open())
        throw invalid_argument("could not open ../data/englishWords.txt");

    string curWord;
    while(!englishWords.eof()){
        englishWords >> curWord;
        answerSet.emplace(curWord);
    }
    englishWords.close();
}

BoardReader::BoardReader(const LString& passed) {
    hand = passed;
    bestX = bestY = 8;
    readerType = UNDEFINED;

    ifstream englishWords;
    englishWords.open("../data/englishWords.txt");
    if(!englishWords.is_open())
        throw invalid_argument("could not open ../data/englishWords.txt");

    string curWord;
    while(!englishWords.eof()){
        englishWords >> curWord;
        answerSet.emplace(curWord);
    }
    englishWords.close();
}

void BoardReader::build_board() {
    ifstream boardFile;
    boardFile.open("../data/Board.csv");
    if(!boardFile.is_open())
        throw invalid_argument("could not open ../data/englishWords.txt");

    string row;
    int rowCount = 0;
    while (getline(boardFile, row)){
        string cell;
        stringstream strStr(row);
        int cellCount = 0;
        LString rowVect;
        while (getline(strStr, cell, ',')){
            if(cell.size() > 0 && isalpha(cell[0])) {
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
}

void BoardReader::print_board() const{
    for (const auto& row : board) {
        for (int i = 0; i < row.length(); ++i) {
            cout << row.read_at(i).LData;
        }
        cout << endl;
    }
}

bool myComp(LString str1, LString str2){
    int sum1 = 0;
    int sum2 = 0;
    for (int i = 0; i < str1.length(); ++i) {
        sum1 += legend[(str1.read_at(i).LData & 31) - 1];
    }

    for (int i = 0; i < str2.length(); ++i) {
        sum2 += legend[(str2.read_at(i).LData & 31) - 1];
    }

    if(sum1 > sum2) {
        return true;
    }
    else if (sum1 < sum2) {
        return false;
    }
    else if(str1.length() > str2.length()){
        return false;
    }
    else if(str1.length() < str2.length()){
        return true;
    }
    else{
        int str1HighValCount = 0;
        int str2HighValCount = 0;
        for (int i = 0; i < str1.length(); ++i) {
            int str1CurVal = legend[(str1.read_at(i).LData & 31) - 1];
            int str2CurVal = legend[(str2.read_at(i).LData & 31) - 1];
            if(str1CurVal > str2CurVal){
                str1HighValCount++;
            }
            else if (str1CurVal < str2CurVal){
                str2HighValCount++;
            }
        }

        if(str1HighValCount > str2HighValCount){
            return false;
        }
        else if(str1HighValCount < str2HighValCount){
            return true;
        }
    }

    return false;
}

void BoardReader::search_board_for_words() {
    ifstream englishWords;
    englishWords.open("../data/englishWords.txt");
    if(!englishWords.is_open())
        throw invalid_argument("could not open ../data/englishWords.txt");

    vector<LString> answers;
    string curWord;
    while(!englishWords.eof()){
        englishWords >> curWord;
        answers.emplace_back(LString(curWord).xVals_to_subscript());
    }
    englishWords.close();

    sort(answers.begin(), answers.end(), myComp);

    int rowSubscript = 0;
    for (const auto& row : board) {
        wordsOfRow[rowSubscript].clear();
        for (auto& word: answers) {
            if(word.isDescendentOf(hand, row) && word.place_into_row(row)) {
                word.set_y_vals_equal_to(rowSubscript);
                wordsOfRow[rowSubscript].push_back(word);
            }
        }
        rowSubscript++;
    }
}

string BoardReader::to_string() const {
    string buffer = "Hand: " + hand.to_string() + "\n";
    buffer += "Best Word: " + bestWord.to_string() + " - " + ::to_string(bestWord.get_horizontal_points() +
                                                                                 perpendicular_points(bestWord));
    buffer += "\nPostion X: " + ::to_string(bestX);
    buffer += "\nPostion Y: " + ::to_string(bestY);
    if(readerType == HORIZONTAL)
        buffer += "\nHorizontal";
    else if (readerType == VERTICAL)
        buffer += "\nVertical";

    return buffer;
}

LString BoardReader::update_best_word(){
    int rowSubscript = 0;
    bestWord.clear();
    for (auto & wordSet : wordsOfRow) {
        for (const auto& word: wordSet) {
            int wordPoints = word.get_horizontal_points() + perpendicular_points(word);
            int bestWordPoints = bestWord.get_horizontal_points() + perpendicular_points(bestWord);

            if (wordPoints > bestWordPoints) {
                bestWord = word;
                if(readerType == HORIZONTAL) {
                    bestX = bestWord[0].x + 1;
                    bestY = rowSubscript + 1;
                }
                else if(readerType == VERTICAL){
                    //x = y, y = 14 - x
                    bestX = (14 - rowSubscript) + 1;
                    bestY = (bestWord[0].x) + 1;
                }
            } else if (wordPoints == bestWordPoints) {
                if (word.length() < bestWord.length() || bestWord.is_empty()) {
                    bestWord = word;
                    if(readerType == HORIZONTAL) {
                        bestX = bestWord[0].x + 1;
                        bestY = rowSubscript + 1;
                    }
                    else if (readerType == VERTICAL){
                        //x = y, y = 14 - x
                        bestX = (14 - rowSubscript) + 1;
                        bestY = (bestWord[0].x) + 1;
                    }
                }
            }
        }
        rowSubscript++;
    }

    return bestWord;
}

LString BoardReader::get_words_of_row(int subscript) {
    LString buffer;
    for (const auto& word: wordsOfRow[subscript]) {
        if(word.is_empty()){
            continue;
        }
        buffer += word + '\n';
    }
    buffer.pop_back();

    return buffer;
}

LString BoardReader::row_to_string(int subscript) {
    LString buffer;
    for (const auto& word: wordsOfRow[subscript]) {
        if(word.is_empty()){
            continue;
        }
        buffer += word;
        buffer += " - " + ::to_string(word.get_horizontal_points() + perpendicular_points(word)) + '\n';
    }
    buffer.pop_back();

    return buffer;
}

void BoardReader::check_perpendicular_compatibility() {
    for (auto & wordSet : wordsOfRow) {
        for (auto& word: wordSet) {
            vector<LString> boardCpy = return_board_with(word);

            for (int i = 0; i < 15; i++) {  //i = x
                LString column;
                if(readerType == HORIZONTAL) {
                    for (int j = 0; j < 15; j++) {  //j = y
                        column += boardCpy[j][i];
                    }
                }
                else if (readerType == VERTICAL){
                    for (int j = 14; j >= 0; j--) {  //j = y
                        column += boardCpy[j][i];
                    }
                }

                vector<LString> colShards = column.break_into_frags();

                for (const auto& shard : colShards) {
                    if(shard.length() > 1 && answerSet.find(shard.to_string()) == answerSet.end()){
                        word.clear();
                    }
                }
            }
        }
    }
}

vector<LString> BoardReader::return_board_with(const LString& toPrint) const{
    vector<LString> boardCpy = board;
    for (int i = toPrint.read_at(0).x; i < toPrint.length() + toPrint.read_at(0).x; i++) {
        if(boardCpy[toPrint.read_at(0).y][i] == ' ')
            boardCpy[toPrint.read_at(0).y][i] = toPrint.read_at(i - toPrint.read_at(0).x);
    }

    return boardCpy;
}

int BoardReader::perpendicular_points(const LString &word) const {
    vector<LString> boardCpy = return_board_with(word);

    int sum = 0;
    for (int i = 0; i < 15; i++) {  //i = x
        LString column;
        for (int j = 0; j < 15; j++) {  //j = y
            column += boardCpy[j][i];
        }

        vector<LString> colShards = column.break_into_frags();

        for (const auto& shard : colShards) {
            if(shard.contains_flag(-1) && shard.length() > 1){
                sum += shard.get_horizontal_points();
            }
        }
    }

    return sum;
}

void BoardReader::to_vertical_reader() {
    vector<LString> boardCpy;
    if(readerType == HORIZONTAL)
        return;
    for (int i = 14; i >= 0; i--) {  //i = x
        LString column;
        for (int j = 0; j < 15; j++) {  //j = y
            column += Letter(board[j][i].LData, j, 14 - i, 1);
        }
        boardCpy.push_back(column);
    }
    board = boardCpy;
    readerType = VERTICAL;
}

void BoardReader::to_horizontal_reader() {
    readerType = HORIZONTAL;
}

void BoardReader::reset_reader() {
    bestX = bestY = 8;
    bestWord.clear();
    hand.clear();
    readerType = UNDEFINED;
    board.clear();

    for (auto & words : wordsOfRow) {
        words.clear();
    }

    answerSet.clear();
    ifstream englishWords;
    englishWords.open("../data/englishWords.txt");
    if(!englishWords.is_open())
        throw invalid_argument("could not open ../data/englishWords.txt");

    string curWord;
    while(!englishWords.eof()){
        englishWords >> curWord;
        answerSet.emplace(curWord);
    }
    englishWords.close();
}
