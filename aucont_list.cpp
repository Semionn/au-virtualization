#include <iostream>
#include <fstream>
#include "cont_list.h"

using namespace std;

int main(int argc, char *argv[]) {
    fstream fin_conts(CONT_LIST_FILE);

    if (!fin_conts.is_open()) {
        return 0;
    }
    
    string line;
    while (getline(fin_conts, line)) {
        cout << line << endl;
    }
}
