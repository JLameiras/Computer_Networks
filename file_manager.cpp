#include "file_manager.h"
#include <dirent.h>
#include <iostream>

bool find_last_game(std::string *PLID , std::string *fname) {
    bool found;
    struct dirent ** filelist ;
    int n_entries;
    char dirname[20], filename[40];
    std::cout << "Getting dirs\n";
    sprintf(dirname, "GAMES/%s/", PLID->c_str()) ;
    n_entries = scandir(dirname, &filelist, 0, alphasort) ;
    found = false;
    if (n_entries <= 0)
        return true;
    else {
        while (n_entries--) {
            if (filelist[n_entries]->d_name[0] != '.' ) {
                sprintf(filename, "GAMES/%s/%s ", PLID->c_str(), filelist[n_entries]->d_name);
                found = true;
            }
            free (filelist[n_entries]);
            if (found) break;
        }
        free(filelist);
        *fname = filename;
    }
    return (found);
}


int find_top_scores (scorelist *list) {
    std::string aux;
    struct dirent **filelist;
    int n_entries, i_file, score, trials, success;
    char fname[50], plid[10], word[10];
    FILE *fp;


    n_entries = scandir("SCORES/", &filelist, nullptr, alphasort);
    i_file=0;
    list->n_scores = n_entries;
    if (n_entries < 0) { return (0); }
    else {
        while (n_entries--) {
            if(filelist[n_entries]->d_name[0] != '.') {
            sprintf(fname, "SCORES/%s", filelist[n_entries]->d_name);
            fp = fopen(fname, "r");

            if (fp != NULL){
                fscanf(fp, "%d %s %s %d %d", &score, plid, word, &success, &trials);
                list->score.push_back(score);
                list->success.push_back(success);
                list->trials.push_back(trials);
                aux = plid;
                list->plid.push_back(aux);
                aux = word;
                list->word.push_back(aux);

                fclose(fp);
                ++i_file;
            }
        }

        free(filelist[n_entries]);
        if(i_file == 10)
            break;
    }
    free(filelist);
}
    return (i_file);
}



