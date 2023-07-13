// RC Project
// Alexandre 100120
// Pedro 99540
#include "file_manager.h"
#include <filesystem>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unordered_map>
#include <arpa/inet.h>
#include <algorithm>
#include <csignal>
#include <valarray>
#include <iosfwd>
#include <sstream>

// Miscellaneous Macros
#define DELIMITER ' '
#define GROUP_NUMBER 84
#define STD_PORT (58000 + GROUP_NUMBER)
#define TCP_BLOCK 512
#define BUFF_SIZE 100
#define SCORE_CODE_SIZE 3
#define TCP_BACKLOG 7
#define PLID_SIZE 6
// Server status codes
#define INV (-2)
#define ERR (-1)
#define OK 0
#define NOK 1
#define WIN 2
#define DUP 3
#define OVR 4
#define FIN 5
#define ACT 6
#define EMPTY 7
#define REP_OK 8
#define REP_NOK 10
// Server game codes
#define PLAY_CODE 1
#define GUESS_CODE 2
#define WIN_CODE 3
#define FAIL_CODE 4
#define QUIT_CODE 5
// File codes
#define GAME_ONGOING 1
#define GAME_FINISHED 2
#define GAME_SCORE 3
#define GAME_TOPSCORE 4
#define GAME_STATE 5
// Format strings
#define DATE_TIME_GAME_FORMAT "%Y%m%d_%H%M%S"
#define DATE_TIME_SCORE_FORMAT "%d%m%Y_%H%M%S"
// Game operations
#define START 1
#define PLAY 2
#define GUESS 3
#define SCORE 4
#define HINT 5
#define STATE 6
#define QUIT 7
/* #define REVIEW 8 */


int create_dir_games(bool *verbose) {
    int aux;
    if (!std::filesystem::exists("./GAMES/")) {
        aux = mkdir("GAMES", 0777);
        if (aux == -1) {
            if (*verbose) std::cerr << "Boot unsuccessful. Error: Impossible to create GAMES directory.\n";
            return -1;
        }
    }
    return 0;
}


int create_dir_scores(bool *verbose) {
    int aux;
    if (!std::filesystem::exists("./SCORES/")) {
        aux = mkdir("SCORES", 0777);
        if (aux == -1) {
            if (*verbose) std::cerr << "Boot unsuccessful. Error: Impossible to create GAMES directory.\n";
            return -1;
        }
    }
    return 0;
}


void boot_server(int argc, char **argv, std::string *gs_port, bool *verbose) {
    int aux;
    *gs_port = std::to_string(STD_PORT);
    for(int i = 2; i < argc; i++) {
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
                case 'p':
                    aux = strtol(argv[++i], nullptr, 10);
                    *gs_port = std::to_string(aux);
                    if(aux <= 0 || aux >= 65535 || errno == ERANGE || errno == EINVAL) {
                        if(*verbose) std::cerr << "Boot unsuccessful. Error: Invalid GSport.\n";
                        exit(-1);
                    }
                    break;
                case 'v':
                    *verbose = true;
                    break;
                default:
                    if(*verbose) std::cerr << "Boot unsuccessful. Error: Invalid flag.\n";
                    exit(-1);
            }
        }
    }

    // Guarantee existence of necessary directories
    if(create_dir_games(verbose) == -1) exit(-1);
    if(create_dir_scores(verbose) == -1) exit(-1);

    if(*verbose) std::cout << "Boot successful.\n";
}


std::string *translate_status_codes(int code) {
    static std::unordered_map<int, std::string> status_codes = {{INV, "INV"},
                                                                {ERR, "ERR"},
                                                                {OK, "OK"},
                                                                {REP_OK, "OK"},
                                                                {NOK, "NOK"},
                                                                {REP_NOK, "NOK"},
                                                                {WIN, "WIN"},
                                                                {DUP, "DUP"},
                                                                {OVR, "OVR"},
                                                                {FIN, "FIN"},
                                                                {ACT, "ACT"},
                                                                {EMPTY, "EMPTY"}};

    if (status_codes.find(code) == status_codes.end()) return nullptr;
    return &status_codes[code];
}


std::string *translate_game_codes(int code) {
    static std::unordered_map<int, std::string> game_codes = {{PLAY_CODE, "T"},
                                                              {GUESS_CODE, "G"},
                                                              {WIN_CODE, "W"},
                                                              {FAIL_CODE, "F"},
                                                              {QUIT_CODE, "Q"}};

    if (game_codes.find(code) == game_codes.end()) return nullptr;
    return &game_codes[code];
}


std::string *translate_operation_codes(int code) {
    static std::unordered_map<int, std::string> operation_codes = {{START, "START"},
                                                              {PLAY, "PLAY"},
                                                              {GUESS, "GUESS"},
                                                              {SCORE, "SCORE"},
                                                              {HINT, "HINT"},
                                                              {STATE, "STATE"},
                                                              {QUIT, "QUIT"},
                                                              /*{REVIEW, "REVIEW"}*/};
    if (operation_codes.find(code) == operation_codes.end()) return nullptr;
    return &operation_codes[code];
}


int evaluate_request(int request_type, std::string *plid, char *letter, int *trial, std::string *word) {
    if(request_type == SCORE) return 0;
    if(plid->length() != PLID_SIZE || plid->find_first_not_of("0123456789", 0) != std::string::npos) return -1;
    if(request_type == PLAY && (std::isalpha(*letter) == 0 || *trial == -1) ||
    (request_type == GUESS && (word->length() == 0 || *trial == -1))) return -1;
    return 0;
}


/* Builds file names. Receives pointer to string filename, file type, pointer to string with player id, game finish code
 * and reference to current date time and based on the integer type stores in filename the correct file name. */
void create_file_name(std::string *filename, int type, std::string *plid, std::string *score, int game_code, std::tm *date_time) {
    filename->clear(); // Flush filename
    char aux_buffer[BUFF_SIZE];
    if (type == GAME_ONGOING) {
        std::cout << "create_file_name: append (GAMES/GAME)\n"; //TODO debug
        filename->append("./GAMES/GAME_");
        std::cout << "create_file_name: append (plid)\n"; // TODO debug
        filename->append(*plid);
    } else if (type == GAME_FINISHED) {
        filename->append("./GAMES/");
        filename->append(*plid);
        filename->append("/");
        strftime(aux_buffer, BUFF_SIZE, DATE_TIME_GAME_FORMAT, date_time);
        filename->append(aux_buffer);
        filename->append("_");
        filename->append(*translate_game_codes(game_code));
    } else if (type == GAME_SCORE) {
        std::cout << "create_file_name: append (SCORES/)\n"; //TODO debug
        filename->append("SCORES/");
        std::cout << "create_file_name: append (score)\n"; //TODO debug
        filename->append(*score);
        filename->append("_");
        std::cout << "create_file_name: append (plid)\n"; // TODO debug
        filename->append(*plid);
        filename->append("_");
        std::strftime(aux_buffer, BUFF_SIZE, DATE_TIME_SCORE_FORMAT, date_time);
        filename->append(aux_buffer);
    } else if (type == GAME_TOPSCORE) {
        filename->append("./TOPSCORES_");
        filename->append(std::to_string(getpid()));
    } else if (type == GAME_STATE) {
        filename->append("STATE_");
        filename->append(*plid);
    }
    filename->append(".txt");
    std::cout << "create_file_name: append (.txt). File name: " << *filename << "\n"; // TODO debug
}


/* Builds udp response messages based on the received arguments. Returns 0 on success or -1 otherwise.
 * Receives pointer to message, message type, status, and two integers, a pointer to a string word and a pointer to an
 * integer vector.
 * Type: START, PLAY, GUESS, QUIT, REV.
 * Additional arguments:
 *      -Start game response: integer_1 -> number of letters; integer_2 -> maximum errors
 *      -Play letter response: integer_1 -> trial number; integer_2 -> number of positions guessed ; pos -> positions of
 *          guessed letter in word
 *      -Guess word response: integer_1 -> trial number
 *      -Review game response: word -> game's mystery word */
int synthesize_udp_response(std::string *message, int type, int status, int integer_1, int integer_2, std::vector<int> *pos) {
    message->clear(); // Flush message
    if(type == START) {
        message->append("RSG ");
        message->append(*translate_status_codes(status));
        if( status != OK ) { message->append("\n"); return 0; }
        message->append(" ");
        message->append(std::to_string(integer_1));
        message->append(" ");
        message->append(std::to_string(integer_2));
    } else if (type == PLAY) {
        message->append("RLG ");
        message->append(*translate_status_codes(status));
        if( status == ERR) { message->append("\n"); return 0; }
        message->append(" ");
        message->append(std::to_string(integer_1));
        if(status == OK) {
            message->append(" ");
            message->append(std::to_string(integer_2));
            for (int element: *pos) {
                message->append(" ");
                message->append(std::to_string(element));
            }
        }
    } else if (type == GUESS) {
        message->append("RWG ");
        message->append(*translate_status_codes(status));
        if( status == ERR) { message->append("\n"); return 0; }
        message->append(" ");
        message->append(std::to_string(integer_1));
    } else if (type == QUIT) {
        message->append("RQT ");
        message->append(*translate_status_codes(status));
    } else return -1;
    message->append("\n");
    return 0;
}


/* Builds tcp response messages based on the received arguments. Returns 0 on success or -1 otherwise.
 * Receives pointer to message, message type, status, and possibly file descriptors (string name, int size) and pointer.
 * Type: SCORE, HINT, STATE. */
int synthesize_tcp_response(std::string *message, int type, int status, std::string *file_name, int file_size, std::ifstream *file) {
    message->clear(); // Flush message;
    if (type == SCORE) {
        message->append("RSB ");
        message->append(*translate_status_codes(status));
        if(status != OK) {message->append("\n"); return 0;}
    } else if (type == HINT) {
        message->append("RHL ");
        message->append(*translate_status_codes(status));
        if(status != OK) {message->append("\n"); return 0;}
    } else if (type == STATE) {
        message->append("STA ");
        message->append(*translate_status_codes(status));
        if(status != ACT && status != FIN) {message->append("\n"); return 0;}
    } else return -1;
    message->append(" ");
    message->append(*file_name);
    message->append(" ");
    message->append(std::to_string(file_size));
    message->append(" ");
    std::ostringstream holder;
    holder << "File is empty? " << std::to_string(holder.width()) << "\n"  << file->rdbuf();
    std::cout << holder.str();
    message->append(holder.str());
    message->append("\n");
    return 0;
}


/* Receives pointer to problem file and pointers to two strings where a mystery word and a corresponding hint file name
 * are stored.*/
int get_problem(std::fstream *file, std::string *problem_word,  std::string *hint_filename) {
    if(file->eof()) { // Check if pointer reached EOF
        file->seekg(0); // Reset pointer to beginning of problems file
        file->clear();
    }
    // File content format: (word) (hint)
    *file >> *problem_word;
    *file >> *hint_filename;

    return 0;
}


/* Receives mystery word size, returns maximum error number in game. */
int max_errors(int length) {
    if (length < 7) return 7;
    else if (length < 11) return 8;
    else return 9;
}


/* Receives a udp request in the format specified by the rules and returns the request type.
 * Stores any additional arguments to the request as predicted by the rules.
 * If request message format is unexpected ERR is returned. */
int request_auxiliary(char buffer[BUFF_SIZE], std::string *str, char *letter, int *trial, std::string *word) {
    int result;
    *str = strtok(buffer, " \n");
    // Get request type
    if(strcmp(str->c_str(), "GSB") == 0) result = SCORE;
    else {
        if (strcmp(str->c_str(), "SNG") == 0) result = START;
        else if (strcmp(str->c_str(), "PLG") == 0) result = PLAY;
        else if (strcmp(str->c_str(), "PWG") == 0) result = GUESS;
        else if (strcmp(str->c_str(), "QUT") == 0) result = QUIT;
        else if (strcmp(str->c_str(), "GHL") == 0) result = HINT;
        else if (strcmp(str->c_str(), "STA") == 0) result = STATE;
        else return ERR;
        *str = strtok(nullptr, " \n");
        std::cout << "request_auxiliary: PLID: " << *str << " | result: " << std::to_string(result) << "\n"; //TODO debug
    }

    if (result != PLAY && result != GUESS) {
        return result;
    }else if (result == PLAY) *letter = strtok(nullptr, " \n")[0];
    else {
        *word = strtok(nullptr, " \n");
        std::cout << "READ WORD: " << *word << "\n";
    }
    *trial = strtoll(strtok(nullptr, " \n"), nullptr, 10);

    return result;
}


/* Receives a file name and checks if the corresponding file exists and is an ongoing game */
bool player_has_ongoing_name(std::string *name) {
    std::ifstream file(*name);
    if (file.is_open()) {
        std::cout << "Has game.\n";
        file.close();
        return true;
    } else {
        std::cout << "Has NO game.\n";
        return false;
    }
}


/* Receives player id, game code indicating the play type and play arguments. Returns play result. */
int evaluate_play(int game_code, std::fstream *game_file, char *letter, int trial, int *real_trial, std::string *message) {
    bool guess = (game_code == GUESS_CODE);
    int result, error = 0;
    std::string mystery_word, word , aux_1, aux_2;
    *game_file >> mystery_word >> aux_2;
    // Preliminary han result
    if(guess) {
        std::cout << "EVALUATE_PLAY Message: " << *message << " | Word: " << mystery_word << "\n";
        if (strcmp(mystery_word.c_str(), message->c_str()) == 0) result = WIN; // Still has to check trial number
        else result = NOK;
        word = *message;
    } else {
        *message = mystery_word; // Store mystery_word for further use in case of letter hit
        if(mystery_word.find(*letter) != std::string::npos) {
            result = OK; // Must check if letter completes mystery_word
            mystery_word.erase(remove(mystery_word.begin(), mystery_word.end(), aux_2.c_str()[0]), mystery_word.end());
        } else result = NOK;
    }
    // Read through game log in search of duplicate commands and see if mystery_word is complete
    while (*game_file >> aux_1 >> aux_2) {
        (*real_trial)++;
        if (result != DUP) {
            std::cout << aux_1 << "........" << aux_2 << "............";
            if (strcmp(translate_game_codes(GUESS_CODE)->c_str(), aux_1.c_str()) == 0) {
                std::cout << "Guess trial: " << std::to_string(*real_trial) << " | Player trial: "
                          << std::to_string(trial) << "\n";
                if (guess && strcmp(aux_2.c_str(), word.c_str()) == 0) {
                    std::cout << "EVALUATE_PLAY GUESS\n";
                    if (*real_trial == trial && !(*game_file >> aux_1)) return REP_NOK;
                    else result =  DUP;
                } else if (*real_trial == trial) result = INV;
                else error++;
            } else if (strcmp(translate_game_codes(PLAY_CODE)->c_str(), aux_1.c_str()) == 0) {
                std::cout << "Player trial: " << std::to_string(trial) << "\n";
                std::cout << "Play trial: " << std::to_string(*real_trial) << "\n";
                if (!guess && aux_2.c_str()[0] == *letter) {
                    if (*real_trial == trial && !(*game_file >> aux_1)) return result == OK ? REP_OK : REP_NOK;
                    else result = DUP;
                } else if (*real_trial == trial) result = INV;
                else if (!guess && (mystery_word.find(aux_2.c_str()) != std::string::npos)) {
                    mystery_word.erase(remove(mystery_word.begin(), mystery_word.end(), aux_2.c_str()[0]),
                                       mystery_word.end());
                } else error++;
            }
        }
    }
    (*real_trial)++;
    if (!guess && (mystery_word.find(*letter) != std::string::npos)) {
        mystery_word.erase(remove(mystery_word.begin(), mystery_word.end(), *letter), mystery_word.end());
        std::cout << "evaluate_play WIN: " << *letter << "\n";
        if (mystery_word.length() == 0) result = WIN;
    }
    std::cout << "Guess trial: " << std::to_string(*real_trial)<< " | Player trial: " << std::to_string(trial) << "\n";
    if(*real_trial != trial) return INV;
    // Check if game over conditions were reached
    if(result == NOK && error == max_errors(message->length() - 1)) return OVR;

    return result;
}


/* Receives a game log pointer to integer success and trials and pointer to string score. Analyzes game log, calculating
 * average score and storing it in score and retrieving number of successful plays and total play number. */
void calculate_game_score(std::fstream *game_file, int *success, int *trials, std::string *score) {
    int s;
    double aux, percentage;
    std::string mystery_word, aux_1, aux_2;
    *success = 0, *trials = 0;
    //Get mystery word
    *game_file >> mystery_word;
    *game_file >> aux_1;
    while(*game_file >> aux_1 >> aux_2) {
        std::cout << "Calculate_Score: GAME CODE: " << aux_1 << " | PLAY: " << aux_2 << "\n";
        if(*translate_game_codes(PLAY_CODE) == aux_1) { // Case: play
            if (mystery_word.find(aux_2.c_str()[0]) != std::string::npos) (*success)++;
        }
        else // Case: guess
            if(mystery_word == aux_2) (*success)++;
        (*trials)++;
    }
    // Get, format and store resulting score
    std::cout << "Calculate_Score: File is read. Successes -> " << std::to_string((*success)) << " | Trials -> " << std::to_string(*trials) << "\n"; // TODO debug
    if((*trials) != 0) {
        percentage = 1.0 * (*success) / (*trials); // Get percentage
        s = static_cast<int>(percentage * 100); // Get score
    } else s = 0;
    std::cout << "Calculate Score: Result gotten -> " << std::to_string(s) << "\n"; // TODO debug
    std::ostringstream result;
    result << std::setfill('0') << std::setw(SCORE_CODE_SIZE) << std::to_string(s); // Format score
    std::cout << "Calculate Score: Result gotten -> " << result.str() << "\n"; // TODO debug
    *score = result.str();
}


int create_games_archive_dir(std::string *plid) {
    std::string aux;
    aux.append("GAMES/");
    aux.append(*plid);
    aux.append("/");
    if (!std::filesystem::exists(aux)) {
        aux.clear();
        aux.append("GAMES/");
        aux.append(*plid);
        std::cout << "Game Over: Creating directory GAMES\n"; // TODO debug
        if(mkdir(aux.c_str(), 0777) == -1) return -1;
        std::cout << "Game Over: Created directory success.\n"; // TODO debug
    }
    return 0;
}


/* Receives a player id and a game code indicating status of reaped game. Ends, archives and scores game. */
int game_over(std::string *plid, int game_code) {
    int success, trials;
    std::fstream game_file, aux_file;
    std::string game_file_name, mystery_word, file_name, aux;
    // Get current time
    //TODO:?
    time_t now = time(nullptr);
    tm *time_holder = gmtime(&now);

    // Open game file and create archive file
    if(create_games_archive_dir(plid) == -1) return -1;
    create_file_name(&game_file_name, GAME_ONGOING, plid, nullptr, -1, nullptr);
    game_file.open(game_file_name, std::fstream::in);
    create_file_name(&file_name, GAME_FINISHED, plid, nullptr, game_code, time_holder);
    aux_file.open(file_name, std::fstream::out);
    if (!aux_file.is_open() || !game_file.is_open()) return -1;
    // Archive game file contents
    game_file >> mystery_word >> aux; // Store mystery word
    aux_file << mystery_word << " " << aux << "\n";
    aux.clear();
    while (game_file && aux_file) {
        getline(game_file, aux);
        aux_file << aux << "\n"; //TODO: Necessary?
    }
    aux_file.close();
    // Create score file
    file_name.clear(); // Flush archive file name
    game_file.clear();
    game_file.seekg(0); // Reset game file pointer
    calculate_game_score(&game_file, &success, &trials, &aux);
    create_file_name(&file_name, GAME_SCORE, plid, &aux, -1, time_holder);
    aux_file.open(file_name, std::fstream::out);
    if (!aux_file.is_open()) return -1;
    aux_file << aux << " "<< *plid << " " << mystery_word << " "<< std::to_string(success) << " " << std::to_string(trials) << "\n";
    aux_file.close();
    // Remove game file from GAMES directory
    if (remove(game_file_name.c_str()) != 0) return -1;
    return 0;
}



void create_scoreboard(std::string *file_name, scorelist *scoreboard, int entries) {
    std::ofstream scoreboard_file;
    std::string aux;
    scoreboard_file.open (*file_name);
    if(!scoreboard_file.is_open()) return;
    scoreboard_file << "Transactions found: " << std::to_string(entries) << "\n";
    for (int n = 0; n < entries; n++) {
        aux = std::to_string(scoreboard->score.at(n));
        scoreboard_file.write(aux.c_str(), aux.length());
        scoreboard_file.write(" ", 1);
        scoreboard_file.write(scoreboard->plid.at(n).c_str(), PLID_SIZE);
        scoreboard_file.write(" ", 1);
        scoreboard_file.write(scoreboard->word.at(n).c_str(), scoreboard->word.at(n).length());
        scoreboard_file.write(" ", 1);
        aux = std::to_string(scoreboard->success.at(n));
        scoreboard_file.write(aux.c_str(), aux.length());
        scoreboard_file.write(" ", 1);
        aux = std::to_string(scoreboard->trials.at(n));
        scoreboard_file.write(aux.c_str(), aux.length());
        scoreboard_file.write("\n", 1);
    }
    scoreboard_file.close();
}


/* Processes a start game request from a player. */
void start_game(std::fstream *problem_file, int *status, std::string *plid, std::string *message) {
    std::ofstream game_file;
    std::string mystery_word, hint_file_name, game_file_name;
    int max_errors;

    *status = OK;

    // Check if player has ongoing game
    create_file_name(&game_file_name, GAME_ONGOING, plid, nullptr, -1, nullptr);
    if(player_has_ongoing_name(&game_file_name)) {
        std::cout << "Has game.\n";
        *status = ERR;
        synthesize_udp_response(message, START, *status, -1, -1, nullptr);
        return;

    }
    std::cout << "Doesn't have game.\n";
    problem_file->seekg(0);
    get_problem(problem_file, &mystery_word, &hint_file_name);

    if (mystery_word.length() < 7) max_errors = 7;
    else if (mystery_word.length() < 11) max_errors = 8;
    else max_errors = 9;

    // Create game log file
    game_file.open(game_file_name.c_str());
    if(game_file.is_open()) {
        game_file << mystery_word << " " << hint_file_name;
        synthesize_udp_response(message, START, *status, mystery_word.length(), max_errors, nullptr);
    } else {
        *status = ERR;
        remove(game_file_name.c_str());
        synthesize_udp_response(message, START, *status, mystery_word.length(), max_errors, nullptr);
    }
        //Synthesize message
    std::cout << "Response message: " << *message /*<< "\n"*/; // TODO debug
}


/* Processes a play request from a player. */
void play_game(int *status, std::string *plid, char *letter, int trial, std::string *message) {
    int n = 0, real_trial = 0;
    std::vector<int> pos;
    std::string filename;
    std::fstream game_file;
    // Open game file
    create_file_name(&filename, GAME_ONGOING, plid, nullptr, -1, nullptr);
    if(player_has_ongoing_name(&filename)) {
        game_file.open(filename, std::fstream::in | std::fstream::out);
        if(game_file.is_open()) {
            *status = evaluate_play(PLAY_CODE, &game_file, letter, trial, &real_trial, message);
            std::cout << "play: evaluated -> " << std::to_string(*status) << "\n"; // TODO debug
            if(*status == OK || *status == NOK || *status == WIN || *status == OVR) { // Register changes to game state in log file
                game_file.close(); // game_file reached eof. impossible to write. reset
                game_file.open(filename, std::ios::app);
                std::cout << filename; // TODO debug
                if(game_file.is_open()) game_file << "\nT " << *letter;
                else *status = ERR;
                std::cout << "T " << *letter << "\n"; // TODO debug
            }
            game_file.close();
        }
        else *status = ERR;
    } else *status = ERR;
    // Manage status dependent operations
    if (*status == OK || *status == REP_OK) {
        for (int i = 0; i < message->length(); i++) // Position numbering starting at 1
            if (message->at(i) == *letter) pos.push_back(i + 1), n++;
    } else if (*status == WIN) game_over(plid, WIN_CODE);
    else if (*status == OVR) game_over(plid, FAIL_CODE);
    // Prepare response
    synthesize_udp_response(message, PLAY, *status, real_trial, n, &pos);
    std::cout << "Response message:" << *message << "\n"; // TODO debug

}


/* Processes a guess request from a player. */
void guess_game(int *status, std::string *plid, std::string *word, int trial, std::string *message) {
    int real_trial = 0;
    std::string filename;
    std::fstream game_file;
    // Open game file
    create_file_name(&filename, GAME_ONGOING, plid, nullptr, -1, nullptr);
    if(player_has_ongoing_name(&filename)) {
        game_file.open(filename, std::fstream::in);
        if(game_file.is_open()) {
            std::cout << "GUESS_GAME: " << *word << "\n";
            *status = evaluate_play(GUESS_CODE, &game_file, nullptr, trial, &real_trial, word);
            if (*status == NOK || *status == WIN || *status == OVR) { // Register changes to game state in log file
                game_file.close(); // game_file reached eof. impossible to write. reset
                game_file.open(filename, std::ios::app);
                std::cout << "G " << *word << "\n"; // TODO debug
                if (game_file.is_open()) game_file << "\nG " << *word;
                else *status = ERR;
            }
            else game_file.close();
        } else *status = ERR;
    } else *status = ERR;
    // Manage status dependent operations
    if (*status == WIN) game_over(plid, WIN_CODE);
    else if(*status == OVR) game_over(plid, FAIL_CODE);
    // Prepare response
    synthesize_udp_response(message, GUESS, *status, real_trial, -1, nullptr);
    std::cout << "Response message:" << *message << "\n"; // TODO debug

}


/* Processes a quit request from a player. */
void quit_game(int *status, std::string *plid, std::string *message) {
    std::string game_file_name;
    // Check if player has ongoing game and close game
    create_file_name(&game_file_name, GAME_ONGOING, plid, nullptr, -1, nullptr);
    if(player_has_ongoing_name(&game_file_name) && game_over(plid, QUIT_CODE) == 0) {
        *status = OK;
        synthesize_udp_response(message, QUIT, *status, -1, -1, nullptr);
    } else {
        *status = NOK;
        synthesize_udp_response(message, QUIT, *status, -1, -1, nullptr);
    }
    std::cout << "Response message: " << *message << "\n"; // TODO debug

}

/* Processes a quit request from a player.
void review_game(int *status, std::string *plid, std::string *message) {
    std::fstream game_file;
    std::string aux;
    // Procure game file name and open game file
    create_file_name(&aux, GAME_ONGOING, plid, nullptr, -1, nullptr);
    game_file.open(aux, std::fstream::in);
    if(!game_file.is_open()) { // Check if game exists
        *status = ERR;
        return;
    }
    // Get mystery word
    game_file >> aux;
    synthesize_udp_response(message, REVIEW, -1, -1, -1, nullptr, &aux);
}
*/


void score_game(std::string *message) {
    std::ifstream file_aux;
    std::string file_name;
    scorelist scoreboard;
    int entries;

    create_file_name(&file_name, GAME_TOPSCORE, nullptr, nullptr, -1, nullptr);
    entries = find_top_scores(&scoreboard);

    if (entries > 0) {
        create_scoreboard(&file_name, &scoreboard, entries);
        file_aux.open(file_name, std::fstream::in);
        if(file_aux.is_open()) {
            synthesize_tcp_response(message, SCORE, OK, &file_name, std::filesystem::file_size(file_name), &file_aux);
            file_aux.close();
        } else
            synthesize_tcp_response(message, SCORE, NOK, nullptr, -1, nullptr);
    } else synthesize_tcp_response(message, SCORE, EMPTY, nullptr, -1, nullptr);
    std::cout << "Response message:" << *message << "\n"; // TODO debug
}


void hint_game(std::string *message, std::string *plid) {
    std::string file_name;
    std::ifstream file_aux;
    // Open game log file
    create_file_name(&file_name, GAME_ONGOING, plid, nullptr, -1, nullptr);
    file_aux.open(file_name);
    if(!file_aux.is_open()) {
        synthesize_tcp_response(message, HINT, NOK, nullptr, -1, nullptr);
        std::cout << "Response message:" << *message << "\n"; // TODO debug
        return;
    }
    // Get hint file
    file_aux >> file_name >> file_name;
    file_aux.close();
    file_aux.open(file_name);
    if(file_aux.is_open()){
        synthesize_tcp_response(message, HINT, OK, &file_name, std::filesystem::file_size(file_name), &file_aux);
        file_aux.close();
    } else synthesize_tcp_response(message, HINT, NOK, nullptr, -1, nullptr);
    std::cout << "Response message:" << *message << "\n"; // TODO debug
}


void state_game(std::string *message, std::string *plid) {
    std::string file_name, aux;
    std::ifstream file_aux;
    std::cout << "State: PLID -> " << *plid << "\n"; // TODO debug
    // Open game log file
    create_file_name(&file_name, GAME_ONGOING, plid, nullptr, -1, nullptr);
    std::cout << "State: file_name -> " << file_name << "\n"; // TODO debug
    file_aux.open(file_name);
    if (file_aux.is_open()) {
        std::cout << "State: opened game log file.\n"; // TODO debug
        file_aux >> aux >> aux;
        synthesize_tcp_response(message, STATE, ACT, &file_name, std::filesystem::file_size(file_name), &file_aux);
        file_aux.close();
    } else { // Check if there are archived game files
        std::cout << "State: game log file doesn't exist.\n"; // TODO debug
        bool found;
        found = find_last_game(plid, &file_name);
        if (found) {
            file_aux.open(file_name);
            if (file_aux.is_open()) {
                synthesize_tcp_response(message, STATE, FIN, &file_name, std::filesystem::file_size(file_name), &file_aux);
                file_aux.close();
            } else synthesize_tcp_response(message, STATE, NOK, nullptr, -1, nullptr);
        } else synthesize_tcp_response(message, STATE, NOK, nullptr, -1, nullptr);
    }
    std::cout << "Response message:" << *message << "\n"; // TODO debug
}


/* Receives buffer with a udp message, string pointer and problen file pointer. Analyzes and processes udp message and
 * stores reply in the string pointer's memory address. Returns request type. */
int udp_request_processing(char buffer[BUFF_SIZE], std::string *message, std::fstream *problem_file, std::string *plid) {
    char letter = '\0';
    int trial = -1, status;
    std::string aux_string;
    std::vector<int> pos;
    // Get request type and arguments
    std::cout << buffer << "\n";
    int request_type = request_auxiliary(buffer, plid, &letter, &trial, &aux_string);
    if(evaluate_request(request_type, plid, &letter, &trial, &aux_string) == ERR) {
        std::cout << "Request ERR\n"; // TODO debug
        synthesize_udp_response(message, request_type, ERR, -1, -1, nullptr);
        return request_type;
    }
    // Satisfy request
    std::cout << "READ WORD: " << aux_string << "\n";
    std::cout << "Proceed to satisfy request: " << *translate_operation_codes(request_type) << "\n"; // TODO debug

    if(request_type == START) start_game(problem_file, &status, plid, message);
    else if(request_type == PLAY) play_game(&status, plid, &letter, trial, message);
    else if(request_type == GUESS) guess_game(&status, plid, &aux_string, trial, message);
    else if(request_type == QUIT) quit_game(&status, plid, message);
    /* else if(request_type == REVIEW) review_game(&status, plid, message); */
    std::cout << *message << "\n";
    std::cout << "Satisfied.\n";
    return request_type;
}


/* Receives a tcp request and a pointer to a string, stores response in message. Returns request type. */
int tcp_request_processing(char buffer[BUFF_SIZE], std::string * message, std::string *plid) {
    int request_type;
    // Get request type and arguments
    request_type = request_auxiliary(buffer, plid, nullptr, nullptr, nullptr);
    std::cout << "tcp_request_processing: request_type -> " << std::to_string(request_type) << "\n";
    if(evaluate_request(request_type, plid, nullptr, nullptr, nullptr) != 0) {
        synthesize_tcp_response(message, request_type, NOK, nullptr, -1, nullptr);
        return request_type;
    }
    // Satisfy request
    if (request_type == SCORE) score_game(message);
    else if (request_type == HINT) hint_game(message, plid);
    else if (request_type == STATE) state_game(message, plid);

    return request_type;
}


/* Handles udp communication between server and players.
 * Receives boolean verbose and pointer to string file with word file name. */
int server_udp_driver(bool verbose, std::string *port, std::fstream *file) {
    int fd, errcode, aux = 1;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;
    char buffer[BUFF_SIZE];
    std::string message, plid;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) exit(1);

    //TODO: Check if this is needed
    /*if(verbose)     setsockopt(fd, SOL_SOCKET, IP_PKTINFO, (const void *)&aux , sizeof(int));*/
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&aux , sizeof(int));

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM; // UDP socket
    hints.ai_flags = AI_PASSIVE;

    errcode = getaddrinfo (nullptr, port->c_str(), &hints, &res);
    if (errcode != 0) /*error*/ exit(1);
    n = bind(fd, res->ai_addr, res->ai_addrlen);
    if (n == -1) exit(1);
    while(true) {
        //Flush message
        message.clear();
        // Receive request
        addrlen = sizeof(addr);
        n = recvfrom(fd, buffer, BUFF_SIZE, 0, (struct sockaddr*)&addr, &addrlen);
        if (n == -1) exit(1);
        std::cout << buffer << "\n";
        // Process request
        aux = udp_request_processing(buffer, &message, file, &plid);
        if(verbose) {
            inet_ntop(AF_INET, &(addr.sin_addr), buffer, INET_ADDRSTRLEN);
            if (aux != ERR) std::cout << plid << ":\n";
            std::cout << " -> Request type: " << *translate_operation_codes(aux) << "\n -> IP adress: " << buffer <<
            "\n -> Port: " << std::to_string(addr.sin_port) << "\n\n";
        }
        // Send reply
        std::cout << "Reply: "<< message << "\n";
        n = sendto(fd, message.c_str(), message.length(), 0, (struct sockaddr*)&addr, addrlen);
        if (n == -1) exit(1);
    }
}


/* Handles tcp communication between server and players. */
int server_tcp_driver(bool verbose, std::string *port) {
    // Handle TCP requests
    int fd, new_fd, ret = 1, message_pointer = 0;
    ssize_t n, nw;
    struct sigaction act;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;
    socklen_t addrlen;
    char buffer[TCP_BLOCK];
    pid_t pid;
    std::string plid, message;


    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    if (sigaction(SIGCHLD, &act, nullptr) == -1)
        exit(1);
    // Initialize tcp socket
    if((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        exit(1);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    std::cout << "tcp_driver: port -> " << port->c_str() << " | fd -> " << std::to_string(fd) << "\n";
    if(getaddrinfo(nullptr, port->c_str(), &hints, &res) != 0)
        exit(1);
    std::cout << "tcp_driver: bind socket\n";
    inet_ntop(AF_INET, &(res->ai_addr), buffer, INET_ADDRSTRLEN);
    std::cout << "tcp_driver: res->ai_addr -> " << buffer << " | res->ai_addrlen -> " << std::to_string(fd) << "\n";
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &ret, sizeof(int)) < 0 || setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &ret, sizeof(int)) < 0)
        exit(1);
    if(bind(fd, res->ai_addr, res->ai_addrlen) == -1) {
        std::cout << "tcp_driver: errno -> " << std::to_string(errno) << "\n";
        exit(1);
    }

    std::cout << "tcp_driver: socket bound\n";
    if(listen(fd, TCP_BACKLOG) == -1) /* Create backlog */
        exit(1);
    std::cout << "1\n";

    while(true) {
        // Flush message
        message.clear();
        // Accept client
        addrlen = sizeof(addr);
        do new_fd = accept(fd, (struct sockaddr *) &addr, &addrlen); // Wait for a connection
        while (new_fd == -1 && errno == EINTR);
        std::cout << "tcp_driver: get request\n"; // TODO debug

        if (new_fd == -1) // Check if socket was accepted
            exit(1);
        if ((pid = fork()) == -1) // Create child process to process request
            exit(1);
        // Child process
        else if (pid == 0) {
            close(fd); // Close listen socket
            // Process request
            std::cout << "tcp_driver: get request\n"; // TODO debug

            while ((n = read(new_fd, buffer, TCP_BLOCK)) != 0) { //TODO: Create time limit?
                if (n == -1)
                    exit(1);
                std::cout << "tcp_driver: gotten request -> " << buffer << "\n"; // TODO debug
                ret = tcp_request_processing(buffer, &message, &plid);
                if(verbose) {
                    inet_ntop(AF_INET, &(addr.sin_addr), buffer, INET_ADDRSTRLEN);
                    if (ret != ERR && ret != SCORE) std::cout << plid << ":\n";
                    std::cout << " -> Request type: " << *translate_operation_codes(ret) << "\n -> IP adress: " << buffer <<
                              "\n -> Port: " << std::to_string(addr.sin_port) << "\n\n";
                }
                std::cout << "Response message:" << message << "\n"; // TODO debug
                while (message.length() - message_pointer > 0) {
                    if ((nw = write(new_fd, message.c_str() + message_pointer, n)) <= 0)
                        exit(1);
                    message_pointer += nw;
                }
                //TODO: Delete this?
                /* transfer_file(new_fd, file, file_size); */
            }
            close(new_fd); // Close connected socket
            exit(0);
        }
        // Parent process
        do ret = close(new_fd); while (ret == -1 && errno == EINTR); // Close connected socket
        if (ret == -1) exit(1);
    }

}



int main(int argc, char **argv) {
    if(argc < 2) return 0;
    bool verbose = false;
    std::string gs_port;
    std::fstream problem_file(argv[1]);
    pid_t pid;
    int status;

    if(!problem_file.is_open()) {
        std::cerr << "Error: Failed to open problem file.\n";
        exit(-1);
    }

    // Server boot
    boot_server(argc, argv, &gs_port, &verbose);
    pid = fork();
    if(pid == -1) {
        perror("Error: Unsuccessful fork.\n");
        exit(-1);
    } else if(pid == 0) {
        // Child process -> Handle TCP connections
        server_tcp_driver(verbose, &gs_port);
        exit(0); // Redundant
    } else {
        // Parent process -> Handle UDP connection
        server_udp_driver(verbose, &gs_port, &problem_file);
        if (verbose) std::cout << "Server shutting down.\n Please wait a few moments.\n";
        kill(0, SIGINT); // Politely ask child processes to terminate
        sleep(1);
        // Reap child processes
        do pid = waitpid(-1, &status, WNOHANG); while(pid != -1); // Reap Child process
    }

    return 0;
}

