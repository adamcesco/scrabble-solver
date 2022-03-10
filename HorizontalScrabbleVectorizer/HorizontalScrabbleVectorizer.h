#ifndef SCRABBLEBOT_BOARDREADER_H
#define SCRABBLEBOT_BOARDREADER_H

#include "../ScrabbleVectorizer/ScrabbleVectorizer.h"

class HorizontalScrabbleVectorizer : public ScrabbleVectorizer{
public:
    HorizontalScrabbleVectorizer();
    explicit HorizontalScrabbleVectorizer(const string &passed);

    void build_board(const string &);
    void print_formatted_board() const;
    Type get_vectorizer_type() const{return HORIZONTAL;}
    vector<LString> return_formatted_board() const{return board;}
    void set_board(vector<LString>);            //assumes the passed board is oriented in a horizontal format
    void set_board(vector<string>);             //assumes the passed board is oriented in a horizontal format
    void set_perkBoard(vector<string>);
    void validate_board() const;
    vector<string> return_formatted_perkBoard() const;
    vector<string> return_formatted_char_board() const;
    LString update_best_word();

    string to_string() const;
    void validate_words();
    vector<LString> return_word_set_of(int subscript){return answerSets[subscript];}
    int points_of_best_word(){return points_of_word(bestWord);}
    vector<vector<LString>> return_formatted_answerSets() const;
    vector<LString> return_formatted_board_with(const LString &) const;     //assumes that the passed word is formatted with respect to a horizontal board
};

#endif //SCRABBLEBOT_BOARDREADER_H