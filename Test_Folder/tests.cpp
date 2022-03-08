//
// Created by misc1 on 3/7/2022.
//

#include "catch.hpp"
#include "../HorizontalBoardReader/HorizontalBoardReader.h"
#include "../VerticalBoardReader/VerticalBoardReader.h"
#include <sstream>

TEST_CASE("Testing user-oriented methods", "[ScrabbleReader]"){
    string hand = "GRO?LEE";
    LString word = "ADAM";
    word[0].x = 10;
    word[1].x = 11;
    word[2].x = 12;
    word[3].x = 13;
    word[0].y = 3;
    word[1].y = 3;
    word[2].y = 3;
    word[3].y = 3;

    HorizontalBoardReader defaultReader(hand);
    defaultReader.build_board("../Test_Folder/Board.csv");

    HorizontalBoardReader hReader(hand);
    hReader.build_board("../Test_Folder/Board.csv");

    VerticalBoardReader vReader(hand);
    vReader.build_board("../Test_Folder/Board.csv");

    ifstream boardFile;
    boardFile.open("../Test_Folder/Board.csv");
    if(!boardFile.is_open())
        throw invalid_argument("could not open ../Test_Folder/Board.csv");

    vector<LString> originalBoard;
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
        originalBoard.push_back(rowVect);
        rowCount++;
    }
    boardFile.close();

    char originalPerkBoard[15][15] =   {{'3', ' ', ' ', 'B', ' ', ' ', ' ', '3', ' ', ' ', ' ', 'B', ' ', ' ', '3'},
                                        {' ', '2', ' ', ' ', ' ', 'C', ' ', ' ', ' ', 'C', ' ', ' ', ' ', '2', ' '},
                                        {' ', ' ', '2', ' ', ' ', ' ', 'B', ' ', 'B', ' ', ' ', ' ', '2', ' ', ' '},
                                        {'B', ' ', ' ', '2', ' ', ' ', ' ', 'B', ' ', ' ', ' ', '2', ' ', ' ', 'B'},
                                        {' ', ' ', ' ', ' ', '2', ' ', ' ', ' ', ' ', ' ', '2', ' ', ' ', ' ', ' '},
                                        {' ', 'C', ' ', ' ', ' ', 'C', ' ', ' ', ' ', 'C', ' ', ' ', ' ', 'C', ' '},
                                        {' ', ' ', 'B', ' ', ' ', ' ', 'B', ' ', 'B', ' ', ' ', ' ', 'B', ' ', ' '},
                                        {'3', ' ', ' ', 'B', ' ', ' ', ' ', '2', ' ', ' ', ' ', 'B', ' ', ' ', '3'},
                                        {' ', ' ', 'B', ' ', ' ', ' ', 'B', ' ', 'B', ' ', ' ', ' ', 'B', ' ', ' '},
                                        {' ', 'C', ' ', ' ', ' ', 'C', ' ', ' ', ' ', 'C', ' ', ' ', ' ', 'C', ' '},
                                        {' ', ' ', ' ', ' ', '2', ' ', ' ', ' ', ' ', ' ', '2', ' ', ' ', ' ', ' '},
                                        {'B', ' ', ' ', '2', ' ', ' ', ' ', 'B', ' ', ' ', ' ', '2', ' ', ' ', 'B'},
                                        {' ', ' ', '2', ' ', ' ', ' ', 'B', ' ', 'B', ' ', ' ', ' ', '2', ' ', ' '},
                                        {' ', '2', ' ', ' ', ' ', 'C', ' ', ' ', ' ', 'C', ' ', ' ', ' ', '2', ' '},
                                        {'3', ' ', ' ', 'B', ' ', ' ', ' ', '3', ' ', ' ', ' ', 'B', ' ', ' ', '3'}};
    for (int i = 0; i < 15; ++i) {
        for (int j = 0; j < 15; ++j) {
            if(originalBoard[i][j].LData != ' ')
                originalPerkBoard[i][j] = ' ';
        }
    }


    SECTION("ScrabbleReader::return_formatted_board() and ScrabbleReader::get_raw_board()") {
        REQUIRE(hReader.return_formatted_board() == originalBoard);
        REQUIRE(hReader.get_raw_board() == originalBoard);
        REQUIRE(vReader.return_formatted_board() == originalBoard);
    }

    SECTION("ScrabbleReader::set_board()") {
        hReader.set_board(originalBoard);
        vReader.set_board(originalBoard);

        REQUIRE(hReader.return_formatted_board() == originalBoard);
        REQUIRE(hReader.get_raw_board() == originalBoard);
        REQUIRE(vReader.return_formatted_board() == originalBoard);
    }

    SECTION("ScrabbleReader::return_formatted_board_with()") {
        defaultReader.place_into_board(word);
        vector<LString> testBoard = defaultReader.return_formatted_board();

        REQUIRE(hReader.return_formatted_board_with(word) == testBoard);
        REQUIRE(vReader.return_formatted_board_with(word) == testBoard);
    }

    SECTION("ScrabbleReader::return_formatted_char_board(char**)") {
        vector<string> testVBoard = vReader.return_formatted_char_board();
        vector<string> testHBoard = hReader.return_formatted_char_board();

        for (int i = 0; i < 15; ++i) {
            REQUIRE(testHBoard[i] == originalBoard[i].to_string());
            REQUIRE(testHBoard[i] == originalBoard[i].to_string());
        }
    }

    SECTION("ScrabbleReader::return_formatted_perkBoard()") {
        vector<string> testVPerks = vReader.return_formatted_perkBoard();
        vector<string> testHPerks = hReader.return_formatted_perkBoard();

        for (int i = 0; i < 15; ++i) {
            for (int j = 0; j < 15; ++j) {
                REQUIRE(testVPerks[i][j] == originalPerkBoard[i][j]);
                REQUIRE(testHPerks[i][j] == originalPerkBoard[i][j]);
            }
        }
    }

    SECTION("ScrabbleReader::return_raw_char_board()") {
        vector<string> testVBoard = vReader.return_raw_char_board();
        vector<string> testHBoard = hReader.return_raw_char_board();

        for (int i = 0; i < 15; ++i) {
            for (int j = 0; j < 15; ++j) {
                REQUIRE(testVBoard[i][j] == originalBoard[j][14 - i].LData);
                REQUIRE(testHBoard[i][j] == originalBoard[i][j].LData);
            }
        }
    }

    SECTION("ScrabbleReader::return_raw_char_board()") {
        vector<string> testVPerks = vReader.return_raw_perkBoard();
        vector<string> testHPerks = hReader.return_raw_perkBoard();

        for (int i = 0; i < 15; ++i) {
            for (int j = 0; j < 15; ++j) {
                REQUIRE(testVPerks[i][j] == originalPerkBoard[j][14 - i]);
                REQUIRE(testHPerks[i][j] == originalPerkBoard[i][j]);
            }
        }
    }
}

TEST_CASE("Testing the difference in resulting word sets formed from different filtering methods (separated word filtration and combined word filtration)", "[ScrabbleReader]"){
    string hand = "POIAUD?";

    HorizontalBoardReader hReader(hand);
    hReader.build_board("../Test_Folder/Board.csv");
    hReader.validate_board();
    hReader.clear_wordSets();
    VerticalBoardReader vReader(hand);
    vReader.build_board("../Test_Folder/Board.csv");
    vReader.validate_board();
    vReader.clear_wordSets();
    vReader.search_for_intersecting_words();
    vReader.search_for_tangential_words();
    hReader.search_for_intersecting_words();
    hReader.search_for_tangential_words();
    vReader.validate_words();
    hReader.validate_words();

    HorizontalBoardReader hReader2(hand);
    hReader2.build_board("../Test_Folder/Board.csv");
    hReader2.validate_board();
    hReader2.clear_wordSets();
    VerticalBoardReader vReader2(hand);
    vReader2.build_board("../Test_Folder/Board.csv");
    vReader2.validate_board();
    vReader2.clear_wordSets();
    vReader2.search_for_all_words();
    hReader2.search_for_all_words();
    vReader2.validate_words();
    hReader2.validate_words();

    vector<vector<LString>> vr1WS = vReader.return_formatted_wordSets();
    vector<vector<LString>> vr2WS = vReader2.return_formatted_wordSets();
    vector<vector<LString>> hr1WS = hReader.return_formatted_wordSets();
    vector<vector<LString>> hr2WS = hReader2.return_formatted_wordSets();

    SECTION("Testing the word count of vertical wordSets formed from differently filtrated readers") {
        REQUIRE(vr1WS.size() == vr2WS.size());

        for (int i = 0; i < vr1WS.size(); ++i) {
            REQUIRE(vr1WS[i].size() == vr2WS[i].size());
        }
    }
    SECTION("Testing if every vertical word in the separated filtration method is found within the combined filtration method") {
        for (const auto &vr1WRow: vr1WS) {
            for (const auto &vr1Word: vr1WRow) {
                bool vr1WordIsFound = false;

                for (const auto &vr2WRow: vr2WS) {
                    for (const auto &vr2Word: vr2WRow) {
                        if (vr1Word == vr2Word) {
                            vr1WordIsFound = true;
                            break;
                        }
                    }
                    if (vr1WordIsFound)
                        break;
                }

                REQUIRE(vr1WordIsFound);
            }
        }
    }
    SECTION("Testing if every vertical word in the combined filtration method is found within the separated filtration method"){
        for(const auto &vr2WRow: vr2WS){
            for (const auto &vr2Word: vr2WRow) {
                bool vr2WordIsFound = false;

                for(const auto &vr1WRow: vr1WS){
                    for (const auto &vr1Word: vr1WRow) {
                        if (vr1Word == vr2Word) {
                            vr2WordIsFound = true;
                            break;
                        }
                    }
                    if(vr2WordIsFound)
                        break;
                }

                REQUIRE(vr2WordIsFound);
            }
        }
    }

    SECTION("Testing the word count of vertical wordSets formed from differently filtrated readers") {
        REQUIRE(hr1WS.size() == hr2WS.size());

        for (int i = 0; i < hr1WS.size(); ++i) {
            REQUIRE(hr1WS[i].size() == hr2WS[i].size());
        }
    }

    SECTION("Testing if every horizontal word in the separated filtration method is found within the combined filtration method") {
        for (const auto &hr1WRow: hr1WS) {
            for (const auto &hr1Word: hr1WRow) {
                bool hr1WordIsFound = false;

                for (const auto &hr2WRow: hr2WS) {
                    for (const auto &hr2Word: hr2WRow) {
                        if (hr1Word == hr2Word) {
                            hr1WordIsFound = true;
                            break;
                        }
                    }
                    if (hr1WordIsFound)
                        break;
                }

                REQUIRE(hr1WordIsFound);
            }
        }
    }

    SECTION("Testing if every horizontal word in the combined filtration method is found within the separated filtration method") {
        for(const auto &hr2WRow: hr2WS){
            for (const auto &hr2Word: hr2WRow) {
                bool hr2WordIsFound = false;

                for(const auto &hr1WRow: hr1WS){
                    for (const auto &hr1Word: hr1WRow) {
                        if (hr1Word == hr2Word) {
                            hr2WordIsFound = true;
                            break;
                        }
                    }
                    if(hr2WordIsFound)
                        break;
                }

                REQUIRE(hr2WordIsFound);
            }
        }

    }
}

TEST_CASE("Testing method LString::is_descendent_of", "[LString]"){
    string givenHand = "DOEWJ?K";

    SECTION("Testing different variations of the given word \"DOEWJ?K\" as input"){
        LString toValidate = "JOOKED";
        REQUIRE(toValidate.is_descendent_of(givenHand));

        toValidate = "JOUKED";
        REQUIRE(toValidate.is_descendent_of(givenHand));

        toValidate = "JOWLED";
        REQUIRE(toValidate.is_descendent_of(givenHand));

        toValidate = "WORKED";
        REQUIRE(toValidate.is_descendent_of(givenHand));

        toValidate = "WORK";
        REQUIRE(toValidate.is_descendent_of(givenHand));

        toValidate = "JOKEY";
        REQUIRE(toValidate.is_descendent_of(givenHand));

        toValidate = "JOKERS";
        REQUIRE(!toValidate.is_descendent_of(givenHand));

        toValidate = "JOKING";
        REQUIRE(!toValidate.is_descendent_of(givenHand));

        toValidate = "";
        REQUIRE(!toValidate.is_descendent_of(givenHand));
    }
}

TEST_CASE("Testing method LString::row_is_descendent_of", "[LString]"){
    LString givenHand = "LEO?UDQ";
    LString givenRow = "     R B  D    ";
    givenRow.set_x_vals_to_subscripts();

    SECTION("Testing different variations of the given word \"LEO?UDQ\" with row \"     R B  D    \" as input"){
        LString toValidate = "     R BUDDY   ";
        LString givenWord = "BUDDY";
        givenWord.set_x_vals_to_subscripts();
        givenWord.add_to_x_vals(7);
        REQUIRE(toValidate.row_is_descendent_of(givenHand, givenRow, givenWord));

        toValidate = "     ROB  D    ";
        givenWord = "ROB";
        givenWord.set_x_vals_to_subscripts();
        givenWord.add_to_x_vals(5);
        REQUIRE(toValidate.row_is_descendent_of(givenHand, givenRow, givenWord));

        toValidate = "     R BLED    ";
        givenWord = "BLED";
        givenWord.set_x_vals_to_subscripts();
        givenWord.add_to_x_vals(7);
        REQUIRE(toValidate.row_is_descendent_of(givenHand, givenRow, givenWord));

        toValidate = "     R B  DAD  ";
        givenWord = "DAD";
        givenWord.set_x_vals_to_subscripts();
        givenWord.add_to_x_vals(10);
        REQUIRE(toValidate.row_is_descendent_of(givenHand, givenRow, givenWord));

        toValidate = "     RUB  D    ";
        givenWord = "RUB";
        givenWord.set_x_vals_to_subscripts();
        givenWord.add_to_x_vals(5);
        REQUIRE(toValidate.row_is_descendent_of(givenHand, givenRow, givenWord));

        toValidate = "     R BADDY   ";
        givenWord = "BADDY";
        givenWord.set_x_vals_to_subscripts();
        givenWord.add_to_x_vals(7);
        REQUIRE(!toValidate.row_is_descendent_of(givenHand, givenRow, givenWord));

        toValidate = "     R B  DUCK ";
        givenWord = "DUCK";
        givenWord.set_x_vals_to_subscripts();
        givenWord.add_to_x_vals(10);
        REQUIRE(!toValidate.row_is_descendent_of(givenHand, givenRow, givenWord));

        toValidate = "";
        givenWord = "ROB";
        givenWord.set_x_vals_to_subscripts();
        givenWord.add_to_x_vals(5);
        REQUIRE(!toValidate.row_is_descendent_of(givenHand, givenRow, givenWord));
    }
}
