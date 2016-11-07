#pragma once

#include <iostream>
#include <vector>
#include <string.h>

using namespace std;

const char* CONT_LIST_FILE = "containers";

void remove_cont(int id) {
    ifstream fin(CONT_LIST_FILE);
    vector<string> lines;
    string line;
    while (getline(fin, line)) {
        if (atoi(line.c_str()) != id) {
            lines.push_back(line);
        }
    }
    fin.close();
    ofstream fout(CONT_LIST_FILE);
    fout << "";
    for (int i = 0; i < lines.size(); ++i) {
        fout << lines[i] << endl;
    }
    fout.close();
}
