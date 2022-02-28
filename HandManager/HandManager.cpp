//
// Created by misc1 on 2/26/2022.
//

#include "HandManager.h"

//TODO: make this class support words of any size
//TODO: make boad letters an object

HandManager::HandManager(string hand){
    this->hand = hand;
}

void HandManager::StartPermute(){
    for(int i = 0; i < handPowerSet.size(); i++) {
        string curString = handPowerSet.at(i);
        Permute(curString, 0, curString.length() - 1);
    }
}

void HandManager::Permute(string a, int l, int r){ //permute(str, 0, n-1);, where n is the size or str
    if (l == r)
        possibleAnswers.push_back(a);
    else
    {
        for (int i = l; i <= r; i++){
            swap(a[l], a[i]);
            Permute(a, l+1, r);
            swap(a[l], a[i]);
        }
    }
}

void HandManager::SortManager(){ //sorts hand by character value
    int size = hand.size();
    int handVals[size];

    for(int i = 0; i < size; i++){
        int curVal = map[(hand.at(i) & 31) - 1];
        handVals[i] = curVal;
    }

    sort(handVals, 0, size - 1);

    for(int i = 0; i < size; i++){
        char curChar = char((handVals[i] % 100) | 64);
        hand[i] = curChar;
    }
}

void HandManager::sort(int *arr, int low, int high){
    int mid;
    if (low < high){
        //divide the array at mid and sort independently using merge sort
        mid = (low + high) / 2;
        sort(arr, low, mid);
        sort(arr, mid + 1, high);
        //merge or conquer sorted arrays
        merge(arr, low, high, mid);
    }
}

void HandManager::merge(int *arr, int low, int high, int mid){
    int i, j, k, c[50];
    i = low;
    k = low;
    j = mid + 1;
    while (i <= mid && j <= high) {
        if (arr[i] > arr[j]) {
            c[k] = arr[i];
            k++;
            i++;
        }
        else  {
            c[k] = arr[j];
            k++;
            j++;
        }
    }
    while (i <= mid) {
        c[k] = arr[i];
        k++;
        i++;
    }
    while (j <= high) {
        c[k] = arr[j];
        k++;
        j++;
    }
    for (i = low; i < k; i++)  {
        arr[i] = c[i];
    }
}

void HandManager::PowerSet()
{
    int counter, j;
    unsigned int powSize = pow(2, hand.size());
    vector<string> pointVals[10 * hand.size()];

    for(counter = 0; counter < powSize; counter++)
    {
        string currentSet = "";
        for(j = 0; j < hand.size(); j++)
        {
            if(counter & (1 << j))
                currentSet += hand[j];
        }

        if(currentSet.length() > 0) {
            int sum = 0;
            for(int i = 0; i < currentSet.length(); i++){
                sum += legend[(currentSet.at(i) & 31) - 1];
            }
            pointVals[sum].push_back(currentSet);
        }

    }

    for(int i = (10 * hand.size()) - 1; i >= 0; i--){
        for(int j = 0; j < pointVals[i].size(); j++){
            handPowerSet.push_back(pointVals[i].at(j));
        }
    }
}

void HandManager::toString() const{
    ofstream output;
    output.open("../output.txt");
    if(!output.is_open())
        throw invalid_argument("could not open ../output.txt");
    output << "Current Hand: " << hand << endl;

//    output << "HandManager Power Set: " << endl;
//    for(int i = 0; i < handPowerSet.size(); i++){
//        output << handPowerSet.at(i) << "-";
//
//        int sum = 0;
//        for(int j = 0; j < handPowerSet.at(i).length(); j++){
//            sum += legend[(handPowerSet.at(i).at(j) & 31) - 1];
//        }
//
//        output << sum << endl;
//    }
//
    output << "Possible Answers: " << endl;
    for(int i = 0; i < possibleAnswers.size(); i++){
        output << possibleAnswers.at(i) << "-";

        int sum = 0;
        for(int j = 0; j < possibleAnswers.at(i).length(); j++){
            sum += legend[(possibleAnswers.at(i).at(j) & 31) - 1];
        }

        output << sum << endl;
    }

    output << "Clean Answers: " << endl;
    for(int i = 0; i < cleanAnswers.size(); i++){
        output << cleanAnswers.at(i) << "-";

        int sum = 0;
        for(int j = 0; j < cleanAnswers.at(i).length(); j++){
            sum += legend[(cleanAnswers.at(i).at(j) & 31) - 1];
        }

        output << sum << endl;
    }

    output.close();
}
bool myComp(string str1, string str2){
    int sum1 = 0;
    int sum2 = 0;
    for (int i = 0; i < str1.length(); ++i) {
        sum1 += legend[(str1.at(i) & 31) - 1];
    }

    for (int i = 0; i < str2.length(); ++i) {
        sum2 += legend[(str2.at(i) & 31) - 1];
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
            int str1CurVal = legend[(str1.at(i) & 31) - 1];
            int str2CurVal = legend[(str2.at(i) & 31) - 1];
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

void HandManager::CleanPossibleAnswers(){
    ifstream englishWords;
    englishWords.open("../data/englishWords.txt");
    if(!englishWords.is_open())
        throw invalid_argument("could not open ../data/englishWords.txt");

    unordered_map<string, bool, MyHashFucntion> answers;
    for (int i = 0; i < possibleAnswers.size(); ++i) {
        answers.emplace(possibleAnswers.at(i), false);
    }
    string curWord;
    while(!englishWords.eof()){
        englishWords >> curWord;
        if(answers.find(curWord) != answers.end()){
            answers.at(curWord) = true;
        }
    }
    englishWords.close();

    cleanAnswers.clear();
    for (const auto& it : answers) {
        if(it.second){
            cleanAnswers.push_back(it.first);
        }
    }

    std::sort(cleanAnswers.begin(), cleanAnswers.end(), myComp);
}

void HandManager::PromptFile(string directory){
    ofstream outFile;
    ifstream inFile;
    inFile.open(directory);

    vector<string> words;
    string curWord;
    while(!inFile.eof()){
        inFile >> curWord;
        for (int i = 0; i < curWord.length(); ++i) {
            curWord.at(i) = toupper(curWord.at(i));
        }
        words.push_back(curWord);
    }

    outFile.open(directory);
    for (int i = 0; i < words.size(); ++i) {
        outFile << words.at(i) << endl;
    }
}

string HandManager::GetBestWord(int size) {
    if(cleanAnswers.empty())
        throw invalid_argument("Error in GetBestWord | There are no clean answers.");
    for(const auto& word: cleanAnswers){
        if(word.size() == size)
            return word;
    }
    return "";
}

string HandManager::GetBestWord(int size, char requestedChar, int requestedSubscript) {
    if(cleanAnswers.empty())
        throw invalid_argument("Error in GetBestWord | There are no clean answers.");
    char curChar = toupper(requestedChar);
    for(const auto& word : cleanAnswers){
        if(word.length() <= size && word[requestedSubscript] == curChar)
            return word;
    }
    return "";
}

int HandManager::gradeWord(string passed) {
    const int valLegend[26] = { 1, 3, 3, 2, 1, 4, 2, 4, 1, 8,  5, 1, 3, 1, 1, 3, 10, 1, 1, 1, 1, 4, 4, 8, 4, 10};
    int sum = 0;
    for(char it : passed){
        sum += valLegend[(it & 31) - 1];
    }
    return sum;
}

string HandManager::GetBestWord(int leftPadding, string toFind, int rightPadding) {
    if(cleanAnswers.empty())
        throw invalid_argument("Error in HandManager::GetBestWord(int, string, int) | There are no clean answers.");

    for(const auto& word : cleanAnswers) {
        string slidingWindow = "";
        for(int i = 0; i < word.length(); i++){
            if(slidingWindow.length() > toFind.length())
                slidingWindow.erase(0,1);

            if((toFind == slidingWindow) && ((i - toFind.length()) <= leftPadding) && ((word.length() - i) <= rightPadding))
                return word;

            slidingWindow += word[i];
        }
    }

    return "";
}

string HandManager::GetBestWord(LString passed) {
    //pass a string a letters, fill the string with every possible answer one by one, only overwritting spaces; then clean possible answers and return first word in possible answers
    bool skip = true;
    vector<LString> subLStrings;
    LString curLs;
    for (auto i : passed) {
        if(i.LData != ' ') {
            curLs += i;
            skip = false;
        }
        else if (!curLs.is_empty()){
            subLStrings.push_back(curLs);
            curLs.clear();
        }
    }
    if (skip){return "";}

    vector<string> PAcopy = possibleAnswers;
    possibleAnswers.clear();
    for(auto& word : PAcopy) {        //check if the current word it unchanged at the end of every loop, if it is not then delete it
        for (int i = 0; i < passed.size(); ++i){
            string row = passed.to_string();
            int skipCount = 0;
            for (int j = 0; j < word.size(); ++j) {
                if(i + j + skipCount > passed.size() - 1){break;}

                if (row[i + j + skipCount] == ' ')
                    row[i + j + skipCount] = word[j];
                else {
                    j--;
                    skipCount += 1;
                }
            }
            possibleAnswers.push_back(row);
        }
    }

    PAcopy = possibleAnswers;
    possibleAnswers.clear();
    for(auto& word : PAcopy){  //problem here
        string curStr;
        vector<string> seperatedWords;
        for (auto i: word) {
            if (i != ' ') {
                curStr += i;
                skip = false;
            } else if (!curStr.empty()) {
                seperatedWords.push_back(curStr);
                curStr.clear();
            }
        }
        for (auto curSWord : seperatedWords) {
            LString LcurSWord = curSWord;
            if(LcurSWord.containsIgnorePadding(passed)) {
                possibleAnswers.push_back(curSWord);
            }
        }
    }


    CleanPossibleAnswers();

    if(cleanAnswers.size() > 0)
        return cleanAnswers[0];
    return "";
}
