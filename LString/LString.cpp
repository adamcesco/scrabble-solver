#include "LString.h"

LString::LString(int size) {
    data = new Letter[size];
    eleCount = 0;
    maxCap = size;
}

void LString::increaseMaxCapacity(){
    maxCap *= 2;
    Letter* dataTemp = new Letter[maxCap];
    for (int i = 0; i < eleCount; ++i) {
        dataTemp[i] = data[i];
    }
    delete[] data;
    data = dataTemp;
}

LString::LString(){
    data = new Letter[10];
    eleCount = 0;
    maxCap = 10;
}

LString::LString(const LString& toCpy){
    eleCount = toCpy.eleCount;
    maxCap = toCpy.maxCap;
    data = new Letter[maxCap];

    for (int i = 0; i < eleCount; ++i)
        data[i] = toCpy.data[i];
}

LString& LString::operator=(const LString& toAssign){
    if(this == &toAssign)
        return *this;

    eleCount = toAssign.eleCount;
    maxCap = toAssign.maxCap;
    delete[] data;
    data = new Letter[maxCap];

    for (int i = 0; i < eleCount; ++i)
        data[i] = toAssign.data[i];

    return *this;
}

Letter& LString::operator[](int subscript){
    if(subscript < 0 || subscript >= maxCap)
        throw invalid_argument("invalid subscript for LString::operator[](int) | subscript parameter is invalid");
    return data[subscript];
}

Letter LString::read_at(int subscript) const{
    if(subscript < 0 || subscript >= maxCap)
        throw invalid_argument("invalid subscript for LString::read_at(int) | subscript parameter is invalid");
    return data[subscript];
}

int LString::length() const{
    return eleCount;
}

int LString::read_maxCap() const{
    return maxCap;
}

Letter LString::read_back() const{
    if(eleCount < 1)
        throw invalid_argument("invalid call for LString::back() | eleCount parameter is less than 1");
    return data[eleCount - 1];
}

Letter& LString::back(){
    if(eleCount < 1)
        throw invalid_argument("invalid call for LString::back() | eleCount parameter is less than 1");
    return data[eleCount - 1];
}

int LString::find_pos_of(Letter toFind) const{
    for (int i = 0; i < eleCount; ++i) {
        if(data[i] == toFind)
            return i;
    }

    return -1;
}

LString& LString::push_back(Letter pssd){
    if(eleCount == maxCap)
        increaseMaxCapacity();

    data[eleCount] = pssd;

    eleCount++;

    return *this;
}

LString& LString::push_front(Letter pssd){
    if(eleCount == maxCap)
        maxCap *= 2;

    Letter* dataCpy = new Letter[maxCap];
    int* flagCpy = new int[maxCap];
    dataCpy[0] = pssd;
    flagCpy[0] = -1;
    for (int i = 0; i < eleCount; ++i) {
        dataCpy[i + 1] = data[i];
    }

    delete[] data;
    data = dataCpy;
    eleCount++;

    return *this;
}

LString& LString::pop_back(){
    if(eleCount == 0)
        throw invalid_argument("invalid subscript for LString::pop_front(int) | LString::length is 0");

    if((eleCount - 1) == (maxCap / 2)) {
        maxCap = (maxCap / 2 > 10) ? maxCap / 2 : 10;

        Letter *dataCpy = new Letter[maxCap];
        int* flagCpy = new int[maxCap];
        for (int i = 0; i < eleCount - 1; ++i) {
            dataCpy[i] = data[i];
        }

        delete[] data;
        data = dataCpy;
    }

    eleCount--;

    return *this;
}

LString& LString::pop_front(){
    if(eleCount == 0)
        throw invalid_argument("invalid subscript for LString::pop_front(int) | LString::length is 0");

    if((eleCount - 1) == (maxCap / 2))
        maxCap = (maxCap / 2 > 10) ? maxCap / 2 : 10;

    Letter* dataCpy = new Letter[maxCap];

    for (int i = 1; i < eleCount; ++i) {
        dataCpy[i - 1] = data[i];
    }

    delete[] data;
    data = dataCpy;
    eleCount--;

    return *this;
}

LString& LString::clear(){
    eleCount = 0;
    maxCap = 10;
    delete[] data;
    data = new Letter[10];
    return *this;
}

LString& LString::erase_at(int subscript){
    if(subscript > (eleCount - 1) || subscript < 0)
        throw invalid_argument("invalid subscript for LString::erase_at()");

    if((eleCount - 1) == (maxCap / 2))
        maxCap = (maxCap / 2 > 10) ? maxCap / 2 : 10;

    Letter* dataCpy = new Letter[maxCap];
    int* flagCpy = new int[maxCap];

    for (int i = 0; i < subscript; ++i) {
        dataCpy[i] = data[i];
    }
    for (int i = (subscript + 1); i < eleCount; ++i) {
        dataCpy[i - 1] = data[i];
    }

    delete[] data;
    data = dataCpy;
    eleCount--;

    return *this;
}

bool LString::is_empty() const{
    if(eleCount > 0)
        return false;
    return true;
}

bool LString::contains(Letter toFind) const{
    for (int i = 0; i < eleCount; ++i) {
        if(data[i] == toFind)
            return true;
    }

    return false;
}

bool LString::operator==(const LString& dsv1) const{
    if(eleCount != dsv1.eleCount)
        return false;

    for (int i = 0; i < eleCount; i++) {
        if(dsv1.data[i].LData != data[i].LData)
            return false;
    }

    return true;
}

LString &LString::operator=(const string &toAssign) {
    eleCount = toAssign.size();
    maxCap = eleCount * 2;
    delete[] data;
    data = new Letter[maxCap];

    for (int i = 0; i < eleCount; ++i) {
        data[i] = toAssign[i];
    }

    return *this;
}

LString& LString::operator+=(const Letter &pssd){
    if(eleCount == maxCap)
        increaseMaxCapacity();

    data[eleCount] = pssd;

    eleCount++;

    return *this;
}

LString& LString::operator+=(char pssd) {
    if(eleCount == maxCap)
        increaseMaxCapacity();

    data[eleCount] = pssd;

    eleCount++;

    return *this;
}

bool LString::contains(char toFind) const {
    for (int i = 0; i < eleCount; ++i) {
        if(data[i] == toFind)
            return true;
    }

    return false;
}

LString &LString::push_back(char pssd) {
    if(eleCount == maxCap)
        increaseMaxCapacity();

    data[eleCount] = pssd;

    eleCount++;

    return *this;
}

LString &LString::push_front(char pssd) {
    if(eleCount == maxCap)
        maxCap *= 2;

    Letter* dataCpy = new Letter[maxCap];
    int* flagCpy = new int[maxCap];
    dataCpy[0] = pssd;
    flagCpy[0] = -1;
    for (int i = 0; i < eleCount; ++i) {
        dataCpy[i + 1] = data[i];
    }

    delete[] data;
    data = dataCpy;
    eleCount++;

    return *this;
}

string LString::to_string() const{
    string temp;
    for(int i = 0; i < eleCount; i++)
        temp += data[i].LData;
    return temp;
}

bool LString::contains(string pssd) const {
    LString passed = pssd;
    int pssedlength = passed.length();
    int thisLen = this->eleCount;
    if (pssedlength > thisLen)  //checks to see if passed length is greater than this->data, if so then it returns false
        return false;
    else if (pssedlength == 0 && thisLen == 0)  //if both this->data and passed have no content, then returns true
        return true;

    LString slidingWindow;
    for (int i = 0; i < passed.eleCount; i++) {    //this grows "slidingWindow" to the same length to "passed", then continuously slides the content through "slidingWindow" until "passed" is found
        slidingWindow += data[i];
        if (slidingWindow.eleCount > pssedlength)
            slidingWindow = slidingWindow.pop_front();

        if (slidingWindow == passed)    //if "passed" is found, then returns true
            return true;
    }

    return false;
}

LString::LString(const string &toCpy) {
    eleCount = toCpy.size();
    maxCap = eleCount * 2;
    data = new Letter[maxCap];

    for (int i = 0; i < eleCount; ++i) {
        data[i] = toCpy[i];
    }
}

bool LString::contains(LString passed) const {
    if (passed.eleCount > eleCount)  //checks to see if passed length is greater than this->data, if so then it returns false
        return false;
    else if (passed.eleCount == 0 && eleCount == 0)  //if both this->data and passed have no content, then returns true
        return true;

    LString slidingWindow;
    for (int i = 0; i < eleCount; i++) {    //this grows "slidingWindow" to the same length to "passed", then continuously slides the content through "slidingWindow" until "passed" is found
        slidingWindow += data[i];
        if (slidingWindow.eleCount > passed.eleCount)
            slidingWindow = slidingWindow.pop_front();

        if (slidingWindow == passed)    //if "passed" is found, then returns true
            return true;
    }

    return false;
}

LString &LString::operator=(char* toAssign) {
    eleCount = strlen(toAssign);
    maxCap = (eleCount > 5) ? eleCount * 2: 10;
    delete[] data;
    data = new Letter[maxCap];

    for (int i = 0; i < eleCount; ++i) {
        data[i] = toAssign[i];
    }

    return *this;
}

bool LString::containsIgnorePadding(LString passed) const {
    vector<LString> subLStrs;
    LString curLs;
    for (auto i : passed) {
        if(i.LData != ' ') {
            curLs += i;
        }
        else if (!curLs.is_empty()){
            subLStrs.push_back(curLs);
            curLs.clear();
        }
    }
    if (!curLs.is_empty())
        subLStrs.push_back(curLs);

    LString slidingWindow;
    const LString pssdCpy = passed;
    LString thisCpy = *this;
    thisCpy.add_to_x_vals(passed.back().x + 1);
    for (int i = 0; i < eleCount + passed.eleCount; i++) {
        slidingWindow += thisCpy.data[i];

        if (slidingWindow.eleCount > passed.eleCount) {
            slidingWindow.pop_front();
        }
        thisCpy.add_to_x_vals(-1);

        for (int j = passed.eleCount - 1; j >= passed.eleCount - slidingWindow.eleCount; --j) {
            if (passed[j] == ' ')
                passed[j] = slidingWindow[(slidingWindow.eleCount - 1) - ((passed.eleCount - 1) - j)];
        }
        if (passed == pssdCpy && subLStrs.size() > 1)
            continue;

        vector<LString> brokenWindow;
        LString curLsWindow;
        for (auto it: passed) {
            if (it.LData != ' ') {
                curLsWindow += it;
            } else if (!curLsWindow.is_empty()) {
                if(curLsWindow.contains_flag(1))
                    brokenWindow.push_back(curLsWindow);
                curLsWindow.clear();
            }
        }
        if (!curLsWindow.is_empty()) {
            if (curLsWindow.contains_flag(1))
                brokenWindow.push_back(curLsWindow);
        }

        for (auto word: brokenWindow) {
                bool skip = false;
                for (auto subWord: subLStrs) {
                    if (word == subWord) {
                        skip = true;
                        break;
                    }
                }
                if (skip)
                    continue;
            if (*this == word) {
                return true;
            }
        }

        passed = pssdCpy;
    }

    return false;
}

bool LString::row_is_descendent_of(const string& hand, const LString& row, LString& word) {
    if(eleCount == 0)
        return false;

    int sumMap[123];
    int letterCount[123];
    int blankCount = 0;
    for (int i = 0; i < 123; ++i) {
        sumMap[i] = 0;
        letterCount[i] = 0;
    }

    for (int i = 0; i < hand.length(); ++i) {
        if(hand[i] == '?')
            blankCount++;
        sumMap[abs(toupper(hand[i]))]++;
    }
    for (int i = word[0].x; i < word.length() + word[0].x; ++i)
        sumMap[abs(toupper(row.read_at(i).LData))]++;

    for (int i = word[0].x; i < word.length() + word[0].x; ++i)
        letterCount[abs(toupper(data[i].LData))]++;

    sumMap[32] = 0;
    letterCount[32] = 0;

    for (int i = word[0].x; i < word.length() + word[0].x; ++i){
        char curChar = abs(toupper(data[i].LData));
        if(sumMap[curChar] < letterCount[curChar] && blankCount == 0)
            return false;
        else if (sumMap[curChar] < letterCount[curChar]) {
            word[i - word[0].x].val = 0;
            word[i - word[0].x].isBlank = true;
            blankCount--;
            letterCount[curChar]--;
            i--;
        }
    }

    return true;
}

LString LString::operator+(const LString &pssd) const{
    LString temp(*this);
    for (int i = 0; i < pssd.eleCount; ++i)
        temp.push_back(pssd.read_at(i));

    return temp;
}

LString::LString(char* toCpy) {
    eleCount = strlen(toCpy);
    maxCap = eleCount * 2;
    data = new Letter[maxCap];

    for (int i = 0; i < eleCount; ++i) {
        data[i] = toCpy[i];
    }
}

int LString::get_horizontal_points(string passed) {
    const int valLegend[26] = { 1, 3, 3, 2, 1, 4, 2, 4, 1, 8,  5, 1, 3, 1, 1, 3, 10, 1, 1, 1, 1, 4, 4, 8, 4, 10};
    int sum = 0;
    for(char it : passed)
        sum += valLegend[(it & 31) - 1];
    return sum;
}

int LString::get_letter_points() const{
    int sum = 0;
    for (int i = 0; i < eleCount; ++i)
        sum += data[i].val;
    return sum;
}

LString& LString::set_x_vals_to_subscripts() {
    for (int i = 0; i < eleCount; ++i)
        data[i].x = i;

    return *this;
}

LString &LString::add_to_x_vals(int passed) {
    for (int i = 0; i < eleCount; ++i)
        data[i].x += passed;

    return *this;
}

LString &LString::set_x_vals_equal_to(int passed) {
    for (int i = 0; i < eleCount; ++i)
        data[i].x = passed;

    return *this;
}

bool LString::contains_flag(int passed) const{
    for (int i = 0; i < eleCount; ++i) {
        if(data[i].flag == passed)
            return true;
    }
    return false;
}

vector<LString> LString::break_into_frags() const{
    vector<LString> fragments;
    LString curFragment;
    for (int i = 0; i < eleCount; ++i) {
        if(data[i].LData != ' ') {
            curFragment += data[i];
        }
        else if (curFragment.eleCount != 0){
            fragments.push_back(curFragment);
            curFragment.clear();
        }
    }
    if (curFragment.eleCount != 0)
        fragments.push_back(curFragment);

    return fragments;
}

LString& LString::operator+=(const LString& toAppend) {
    maxCap = (toAppend.eleCount + eleCount) * 2;
    Letter* temp = new Letter[maxCap];
    for (int i = 0; i < eleCount; ++i)
        temp[i] = data[i];

    for (int i = 0; i < toAppend.eleCount; ++i)
        temp[i + eleCount] = toAppend.data[i];

    delete[] data;
    data = temp;
    eleCount = toAppend.eleCount + eleCount;

    return *this;
}

LString LString::operator+(const Letter& toAppend) const {
    LString temp(*this);
    temp.push_back(toAppend);
    return temp;
}

LString LString::operator+(char toAppend) const {
    LString temp(*this);
    temp.push_back(toAppend);
    return temp;
}

LString &LString::set_y_vals_equal_to(int passed) {
    for (int i = 0; i < eleCount; ++i)
        data[i].y = passed;

    return *this;
}

bool LString::is_blank_LStr() const {
    for (int i = 0; i < eleCount; ++i) {
        if(isalpha(data[i].LData)) {
            return false;
        }
    }
    return true;
}

bool LString::is_descendent_of(const string& hand) {
    if(eleCount == 0)
        return false;

    int handMap[123];
    int letterCount[123];
    int blankCount = 0;
    for (int i = 0; i < 123; ++i)
        handMap[i] = letterCount[i] = 0;

    for (int i = 0; i < hand.length(); ++i) {
        if(hand[i] == '?')
            blankCount++;
        handMap[abs(toupper(hand[i]))]++;
    }

    for (int i = 0; i < eleCount; ++i)
        letterCount[abs(toupper(data[i].LData))]++;

    handMap[32] = 0;
    letterCount[32] = 0;

    for (int i = 0; i < eleCount; ++i) {
        char curChar = abs(toupper(data[i].LData));
        if(handMap[curChar] < letterCount[curChar] && blankCount == 0)
            return false;
        else if (handMap[curChar] < letterCount[curChar]) {
            data[i].val = 0;
            data[i].isBlank = true;
            blankCount--;
            letterCount[curChar]--;
            i--;
        }
    }

    return true;
}

LString& LString::convert_vertical_format_to_horizontal() {
    for (int i = 0; i < eleCount; ++i) {
        int temp = data[i].x;
        data[i].x = 14 - data[i].y;
        data[i].y = temp;
    }

    return *this;
}

LString &LString::convert_horizontal_format_to_vertical() {
    for (int i = 0; i < eleCount; ++i) {
        int temp = data[i].x;
        data[i].x = data[i].y;
        data[i].y = 14 - temp;
    }

    return *this;
}
