#ifndef RC_DIRENT_H
#define RC_DIRENT_H
#include <cstdio>
#include <cstring>
#include <string>
#include <dirent.h>
#include <vector>


typedef struct scorelist {
    std::vector<int> score;
    std::vector<std::string> plid;
    std::vector<std::string> word;
    std::vector<int> success;
    std::vector<int> trials;
    int n_scores;
} scorelist;

bool find_last_game (std::string *PLID, std::string *fname);
int find_top_scores(scorelist *list);

#endif //RC_DIRENT_H
