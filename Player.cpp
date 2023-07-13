#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <vector>
#include <unistd.h>

#define TIMEOUT_SECONDS 5
#define TIMEOUT_MICROSECONDS 0
#define LOCAL_HOST "127.0.0.1"
#define GROUP_NUMBER 84
#define STD_PORT (58000 + GROUP_NUMBER)
#define BUFF_SIZE 100
#define TCP_READ_BLOCK 512
#define DELIMITER ' '
// Status codes
#define INV (-2)
#define ERR (-1)
#define OK 0
#define NOK 1
#define WIN  2
#define DUP 3
#define OVR 4
#define FIN 5
#define ACT 6
#define EMPTY 7
#define UNEXPECTED_SYNTAX 8
#define UNEXPECTED_STATUS 9
#define TIMEOUT 10
// Operation codes
#define START 1
#define PLAY 2
#define GUESS 3
#define SCORE 4
#define HINT 5
#define STATE 6
#define QUIT 7
/* #define REV 8 */


void initialize_program(int argc, char **argv, std::string *ip, std::string *port) {
    int gs_port = STD_PORT;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'n':
                    *ip = argv[i+1];
                    break;
                case 'p':
                    gs_port = strtol(argv[i+1], nullptr, 10);
                    if(gs_port <= 0 || gs_port >= 65535 || errno == ERANGE || errno == EINVAL) {
                        std::cerr << "Error: Invalid GSport.\n";
                        exit(1);
                    }
                    break;
                default:
                    std::cerr << "Error: Invalid flag.\n";
                    exit(1);
            }
        }
    }
    *port = std::to_string(gs_port);
}


void synthesize_message(std::string *message, int command, std::string *player_id, std::string *command_argument, int trial) {

    switch (command) {
        case START:
            message->append("SNG ");
            break;
        case PLAY:
            message->append("PLG ");
            break;
        case GUESS:
            message->append("PWG ");
            break;
        case SCORE:
            message->append("GSB\n");
            return;
        case HINT:
            message->append("GHL ");
            break;
        case STATE:
            message->append("STA ");
            break;
        case QUIT:
            message->append("QUT ");
            break;
    }

    message->append(*player_id);

    if (command != PLAY && command != GUESS) {
        message->append("\n");
        return;
    }

    message->append(" ");
    message->append(*command_argument);
    message->append(" ");
    message->append(std::to_string(trial));
    message->append("\n");
}


void command_processing(std::string *line, std::string *command, std::string *command_arg) {
    int command_split_index = line->find(DELIMITER);

    // Case: command has no arguments
    if (command_split_index == -1) {
        *command = *line;
        return;
    }

    // Case: command has arguments
    *command = line->substr(0, command_split_index);
    *command_arg = line->substr(command_split_index + 1, line->length() - command_split_index - 1);
}


void build_mystery_word(std::string *word, int word_size) {
    word->push_back('_');
    for (int i = 1; i < word_size; i++) {
        word->push_back(' ');
        word->push_back('_');
    }
}


void fill_mystery_word(std::string *mystery_word, std::vector<int> *positions, char played_letter) {
    for (int position : *positions)
        mystery_word->at((position - 1) * 2) = played_letter;
    positions->clear();
}


void finish_mystery_word(std::string *mystery_word, char played_letter) {
    for (int i = 0; i < mystery_word->length(); i++)
        if (mystery_word->at(i) == '_') mystery_word->at(i) = played_letter;
}


void reveal_mystery_word(std::string *mystery_word, std::string solution) {
    for (int i = 0; i < solution.length(); i++) {
        mystery_word->at(i * 2) = solution.at(i);
    }
}


int start_server_response_processing(char buffer[BUFF_SIZE], int *n_letters, int *max_errors) {
    std::string status;

    if(strcmp(strtok(buffer, " "), "RSG") != 0) return UNEXPECTED_SYNTAX;

    status = strtok(nullptr, " ");

    if(strcmp(status.c_str(), "OK") == 0) {
        *n_letters = strtol(strtok(nullptr, " "), nullptr, 10);
        *max_errors = strtol(strtok(nullptr, " "), nullptr, 10);
        return OK;
    } else if (strcmp(status.c_str(), "NOK\n") == 0){
        return NOK;
    }

    return UNEXPECTED_STATUS;
}


int play_server_response_processing(char buffer[BUFF_SIZE], int trial, int *hits, std::vector<int> *positions) {
    if(strcmp(strtok(buffer, " "), "RLG") != 0) return UNEXPECTED_SYNTAX;
    // Switch for game state
    std::string status = strtok(nullptr, " ");
    // Case: letter is in word
    if(strcmp(status.c_str(), "OK") == 0) {
        if(trial != strtol(strtok(nullptr, " "), nullptr, 10))
            return INV;
        *hits = strtol(strtok(nullptr, " "), nullptr, 10);
        for(int i = 0; i < *hits; i++)
            positions->push_back(strtol(strtok(nullptr, " "), nullptr, 10));
        return OK;
    }
    // Case: letter completes word
    if(strcmp(status.c_str(), "WIN") == 0) {
        return WIN;
    }
    // Case: letter is duplicate from previous trial
    if(strcmp(status.c_str(), "DUP") == 0) {
        return DUP;
    }
    // Case: letter not in word
    if(strcmp(status.c_str(), "NOK") == 0) {
        return NOK;
    }
    // Case: letter not in word game over
    if(strcmp(status.c_str(), "OVR") == 0) {
        return OVR;
    }
    // Case: trial number not valid
    if(strcmp(status.c_str(), "INV") == 0) {
        return INV;
    }
    // Case: syntax of PLG incorrect
    if(strcmp(status.c_str(), "ERR") == 0) {
        return ERR;
    }

    return UNEXPECTED_STATUS;
}


int guess_server_response_processing(char buffer[BUFF_SIZE]) {
    if(strcmp(strtok(buffer, " "), "RWG") != 0) return UNEXPECTED_SYNTAX;

    std::string status = strtok(nullptr, " ");

    if(strcmp(status.c_str(), "WIN") == 0) {
        return WIN;
    }
    if(strcmp(status.c_str(), "NOK") == 0) {
        return NOK;
    }
    if(strcmp(status.c_str(), "OVR") == 0) {
        return OVR;
    }
    if(strcmp(status.c_str(), "INV") == 0) {
        return INV;
    }
    if(strcmp(status.c_str(), "ERR") == 0) {
        return ERR;
    }

    return UNEXPECTED_STATUS;
}



int quit_server_response_processing(char buffer[BUFF_SIZE]) {
    if(strcmp(strtok(buffer, " "), "RQT") != 0) return UNEXPECTED_SYNTAX;
    std::string status = strtok(nullptr, "\n");


    if(strcmp(status.c_str(), "OK") == 0) {
        return OK;
    } else if(strcmp(status.c_str(), "ERR") == 0) {
        return ERR;
    } else if(strcmp(status.c_str(), "NOK") == 0) {
        return NOK;
    }
    return UNEXPECTED_STATUS;
}


int initialize_socket_udp(int *udp_socket_fd, struct addrinfo *hints_udp, struct addrinfo **res_udp, struct sockaddr_in *addr_udp, socklen_t *addrlen_udp, struct timeval *timeout, std::string *ip, std::string *port) {
    int errcode;
    *udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*udp_socket_fd == -1) {
        std::cerr << "Error: Unable to create udp socket.\n";
        exit(1);
    }

    memset(hints_udp, 0, sizeof(*hints_udp));
    hints_udp->ai_family = AF_INET;
    hints_udp->ai_socktype = SOCK_DGRAM;

    errcode = getaddrinfo(ip->c_str(), port->c_str(), hints_udp, res_udp);
    if(errcode != 0) {
        std::cerr << "Error: Unable to get server address information.\n";
        exit(1);
    }

    *addrlen_udp = sizeof(*addr_udp);

    setsockopt(*udp_socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)timeout, sizeof(struct timeval));

    return 0;
}


int message_server_udp(int *udp_socket_fd, struct addrinfo *res_udp, struct sockaddr_in *addr_udp, socklen_t *addrlen_udp, std::string *message, int command, int *pointer1, int *pointer2, std::vector<int> *positions) {
    ssize_t n;
    char buffer[BUFF_SIZE];

    n = sendto(*udp_socket_fd, message->c_str(), message->length(), 0, res_udp->ai_addr, res_udp->ai_addrlen);
    if (n == -1) {
        std::cerr << "Error: UDP connection player side failed.\n";
        exit(1);
    }

    n = recvfrom(*udp_socket_fd, buffer, BUFF_SIZE, 0, (struct sockaddr *) addr_udp, addrlen_udp);
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return TIMEOUT;
        else {
            std::cerr << "Error: UDP connection server side failed.\n";
        }
        return ERR;
    }

    if (command == START) {
        return start_server_response_processing(buffer, pointer1, pointer2);
    } else if (command == PLAY) {
        return play_server_response_processing(buffer, *pointer1, pointer2, positions);
    } else if (command == GUESS) {
        return guess_server_response_processing(buffer);
    } else if (command == QUIT) {
        return quit_server_response_processing(buffer);
    }
    return UNEXPECTED_SYNTAX;
}


int initialize_socket_tcp(int *tcp_socket_fd, struct addrinfo *hints_tcp, struct addrinfo **res_tcp, std::string *ip, std::string *port) {
    int errcode;
    ssize_t n;
    *tcp_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*tcp_socket_fd == -1) {
        std::cerr << "Error: Unable to create tcp socket.\n";
        exit(1);
    }

    memset(hints_tcp, 0, sizeof (*hints_tcp));
    hints_tcp->ai_family = AF_INET;
    hints_tcp->ai_socktype = SOCK_STREAM;

    errcode = getaddrinfo(ip->c_str(), port->c_str(), hints_tcp, res_tcp);
    if(errcode != 0) {
        std::cerr << "Error: Unable to get server tcp address information.\n";
        exit(1);
    }

    n = connect(*tcp_socket_fd, (*res_tcp)->ai_addr, (*res_tcp)->ai_addrlen);
    if(n == -1) {
        std::cerr << "Error: Unable to connect tcp socket.\n";
        exit(1);
    }

    return 0;
}


int create_file_tcp(int *tcp_socket_fd, int *file_size, std::string *file_name) {
    char buffer[TCP_READ_BLOCK];
    int n = 0, r;
    std::ofstream file;
    file.open (*file_name);

    // Read file from tcp socket
    while (n < *file_size) {
        r = read(*tcp_socket_fd, buffer, 1);
        if(r == -1) return NOK;
        file.write(buffer, r);
        n += r;
    }
    //Read '\n' end of message
    for(r = 0; r < 1;) {
        r = read(*tcp_socket_fd, buffer, 1);
        if(r == -1) return NOK;
    }

    file.close();
    return OK;
}


void print_file(std::string *file_name) {
    std::ifstream file;
    file.open (*file_name);

    if (file.is_open())
        std::cout << file.rdbuf();

    file.close();
}


int read_until_space_tcp(int *tcp_socket_fd, char buffer[BUFF_SIZE]) {
    int n, bytes_read = 0;
    do {
        n = read(*tcp_socket_fd, &buffer[bytes_read], 1);
        if (n == -1) {
            std::cerr << "Error: TCP connection player side failed.\n";
            exit(1);
        }
        bytes_read++;
    } while (buffer[bytes_read - 1] != ' ' && buffer[bytes_read - 1] != '\n');

    buffer[bytes_read - 1] = '\0';

    return bytes_read;
}


int tcp_server_response_processing(int *tcp_socket_fd, int command, std::string *filename, int *file_size) {
    bool fin = false;
    int bytes_read = 0;
    char buffer[BUFF_SIZE];

    bytes_read = read_until_space_tcp(tcp_socket_fd, buffer);


    if ((strncmp(buffer, "ERR", bytes_read) == 0)) {
        return ERR;
    }

    // Read status
    bytes_read = read_until_space_tcp(tcp_socket_fd, buffer);


    if (command == SCORE || command == HINT) {
        if (strncmp(buffer, "NOK", bytes_read) == 0)
            return NOK;
        if (strncmp(buffer, "EMPTY", bytes_read) == 0)
            return EMPTY;
    } else if (command == STATE) {
        if (strncmp(buffer, "FIN", bytes_read) == 0) fin = true;
        else if (strncmp(buffer, "NOK", bytes_read) == 0) return NOK;
    }

    //Read File name
    bytes_read = read_until_space_tcp(tcp_socket_fd, buffer);

    filename->clear();
    filename->append(buffer, bytes_read);

    //Read File size
    bytes_read = read_until_space_tcp(tcp_socket_fd, buffer);

    *file_size = strtol(buffer, nullptr, 10);

    if(fin) {
        return FIN;
    }

    return OK;
}


/* Filename returned in message  */
int message_server_tcp(std::string *ip, std::string *port, std::string *message, int *file_size, int command) {
    int tcp_socket_fd, status;
    ssize_t n;
    struct addrinfo hints_tcp, *res_tcp;

    initialize_socket_tcp(&tcp_socket_fd, &hints_tcp, &res_tcp, ip, port);

    n = write(tcp_socket_fd, message->c_str(), message->length());
    if (n == -1) {
        std::cerr << "Error: TCP connection player side failed.\n";
        exit(1);
    }

    // File name is to be stored in message
    message->clear();
    status = tcp_server_response_processing(&tcp_socket_fd, command, message, file_size);

    // OK in state is ACT
    if (status == OK || status == FIN) {
        if (create_file_tcp(&tcp_socket_fd, file_size, message) != OK) {
            std::cerr << "Error: TCP connection player side failed.\n";
        }
    }

    //Close TCP connection
    freeaddrinfo(res_tcp);
    close(tcp_socket_fd);

    return status;
}


int main(int argc, char **argv) {
    int n_letters, max_errors, trial = 1, hits, status, udp_socket_fd, file_size;
    std::vector<int> positions;
    char played_letter = '!';
    std::string player_id, line, command, command_argument, message, mystery_word, solution, port = std::to_string(STD_PORT), ip = LOCAL_HOST;

    socklen_t addrlen_udp;
    struct addrinfo hints_udp, *res_udp;
    struct sockaddr_in addr_udp;
    struct timeval timeout = {TIMEOUT_SECONDS, TIMEOUT_MICROSECONDS};
    if(argc > 1) initialize_program(argc, argv, &ip, &port);

    //Initialize udp connection
    initialize_socket_udp(&udp_socket_fd, &hints_udp, &res_udp, &addr_udp, &addrlen_udp, &timeout, &ip, &port);

    while (true) {
        message.clear();

        //Read player command
        getline(std::cin, line);
        command_processing(&line, &command, &command_argument);


        if (command == "start" || command == "sg") {
            trial = 1;
            player_id = command_argument;

            // Message GS to start new game with player ID
            synthesize_message(&message, START, &player_id, nullptr, trial);

            status = message_server_udp(&udp_socket_fd, res_udp, &addr_udp, &addrlen_udp, &message, START, &n_letters, &max_errors, nullptr);
            if (status == TIMEOUT) {
                std::cerr << "Message timeout. Trying again\n";
                status = message_server_udp(&udp_socket_fd, res_udp, &addr_udp, &addrlen_udp, &message, START, &n_letters, &max_errors, nullptr);
                if (status == TIMEOUT) {
                    std::cerr << "Second timeout. Aborting request.\n";
                    player_id.clear();
                }
            } else if (status == NOK || status == UNEXPECTED_SYNTAX || status == UNEXPECTED_STATUS) {
                player_id.clear();
                if(status == NOK) std::cerr << "Error: Player has ongoing game.\n";
                else std::cerr << "Invalid player ID and/or unknown status received by server.\n";
            } else if (status == OK) {
                build_mystery_word(&mystery_word, n_letters);
                std::cout << "New game started (max " << max_errors
                     << " errors). Guess " << n_letters << " letter word "
                     << mystery_word << "\n";
            }
        }
        if (command == "scoreboard" || command == "sb") {
            synthesize_message(&message, SCORE, &player_id, nullptr, trial);

            status = message_server_tcp(&ip, &port, &message, &file_size, SCORE);

            if (status == EMPTY) {
                std::cout << "Failure. scoreboard: Empty. No game has been won.\n";
            } else if (status == OK) {
                print_file(&message);
                std::cout << "Success. scoreboard file name: " << message << ". Saved in working directory.\n";
            } else if (status == ERR) {
                std::cout << "Failure. scoreboard command error and/or unexpected server response.\n";
            }
        }
        if (command == "play" || command == "pl") {
            // Store letter
            played_letter = command_argument.c_str()[0];

            synthesize_message(&message, PLAY, &player_id, &command_argument, trial);

            status = message_server_udp(&udp_socket_fd, res_udp, &addr_udp, &addrlen_udp, &message, PLAY, &trial, &hits, &positions);
            if (status == TIMEOUT) {
                std::cerr << "Message timeout. Trying again\n";
                status = message_server_udp(&udp_socket_fd, res_udp, &addr_udp, &addrlen_udp, &message, START, &n_letters, &max_errors, nullptr);
                if (status == TIMEOUT) {
                    std::cerr << "Second timeout. Aborting request.\n";
                    played_letter = '!';
                    continue;
                }
            }
            if (status == OK) {
                fill_mystery_word(&mystery_word, &positions, played_letter);
                std::cout << "play: The letter " << played_letter << " is part of word.\n" << mystery_word << "\n";
                trial++;
            } else if (status == WIN) {
                finish_mystery_word(&mystery_word, played_letter);
                std::cout << "play: YOU WON!!! The word is " << mystery_word << "!\n";
                player_id.clear();
                mystery_word.clear();
            } else if (status == DUP) {
                std::cout << "play: The letter " << played_letter << " has already been played.\n";
            } else if (status == NOK) {
                std::cout << "play: The letter " << played_letter << " is not part of the word to be guessed.\n";
                trial++;
            } else if (status == OVR) {
                std::cout << "play: The letter " << played_letter << " is not part of the word to be guessed. 0 attempts left.\nGAME OVER.\n";
                player_id.clear();
                mystery_word.clear();
            } else if (status == INV) {
                std::cerr << "play: Error. Unexpected inconsistency in the play trial number.\n";
            } else if (status == ERR) {
                std::cerr << "play: Error. Command invalid.\n";
            } else if (status == UNEXPECTED_SYNTAX || status == UNEXPECTED_STATUS) {
                std::cerr << "play: Unexpected message received from the server.\n";
            }

        }
        if (command == "guess" || command == "gw") {

            synthesize_message(&message, GUESS, &player_id, &command_argument, trial);

            status = message_server_udp(&udp_socket_fd, res_udp, &addr_udp, &addrlen_udp, &message, GUESS, nullptr, nullptr, nullptr);
            if (status == TIMEOUT) {
                std::cerr << "Message timeout. Trying again\n";
                status = message_server_udp(&udp_socket_fd, res_udp, &addr_udp, &addrlen_udp, &message, START, &n_letters, &max_errors, nullptr);
                if (status == TIMEOUT) {
                    std::cerr << "Second timeout. Aborting request.\n";
                    continue;
                }
            }
            if (status == WIN) {
                reveal_mystery_word(&mystery_word, command_argument);
                std::cout << "guess: WELL DONE! You guessed: " << mystery_word << "\n";
                player_id.clear();
                mystery_word.clear();
            } else if (status == NOK) {
                std::cout << "guess: Word guessed incorrectly.\n";
                trial++;
            } else if (status == OVR) {
                std::cout << "guess: Word guessed incorrectly. Game over.\n";
                player_id.clear();
                mystery_word.clear();
            } else if (status == INV) {
                std::cout << "guess: Incorrect trial number.\n";
            } else if (status == ERR) {
                std::cout << "guess: Incorrect syntax and/or no ongoing game for the specified player.\n";
            } else if (status == UNEXPECTED_SYNTAX || status == UNEXPECTED_STATUS) {
                std::cerr << "guess: Unexpected message received from the server.\n";
            }
        }
        if (command == "hint" || command == "h") {
            synthesize_message(&message, HINT, &player_id, nullptr, trial);

            status = message_server_tcp(&ip, &port, &message, &file_size, HINT);

            if (status == OK) {
                std::cout << "Success. hint: file name: " << message << "." << " File size: " <<  file_size << " Bytes. Saved in working directory\n";
            } else if (status == NOK) {
                std::cout << "Failure. hint: No file to be sent.\n";
            } else if (status == ERR) {
                std::cout << "Failure. Unexpected server response.\n";
            }
        }
        if (command == "state" || command == "st") {
            synthesize_message(&message, STATE, &player_id, nullptr, trial);
            status = message_server_tcp(&ip, &port, &message, &file_size, STATE);
            //ACT
            if (status == OK) {
                print_file(&message);
                std::cout << "Success. state: Current game can be checked in file: " << message << ". File size: " << file_size << " Bytes. Saved in working directory.\n";
            } else if (status == FIN) {
                std::cout << "Success. state: Last played game can be checked in file: " << message << ". File size: " << file_size << " Bytes. Saved in working directory.\n";
                player_id.clear();
                mystery_word.clear();
            } else if (status == NOK){
                std::cout << "Failture. state: No ongoing or finished games for the given player.\n";
            } else if (status == ERR){
                std::cout << "Failure. Unexpected server response.\n";
            }
        }
        if (command == "quit" || command == "exit") {
            if (!player_id.empty()) {
                synthesize_message(&message, QUIT, &player_id, nullptr, trial);

                status = message_server_udp(&udp_socket_fd, res_udp, &addr_udp, &addrlen_udp, &message, QUIT, nullptr,
                                            nullptr, nullptr);
                if (status == TIMEOUT) {
                    std::cerr << "Message timeout. Trying again\n";
                    status = message_server_udp(&udp_socket_fd, res_udp, &addr_udp, &addrlen_udp, &message, START,
                                                &n_letters, &max_errors, nullptr);
                    if (status == TIMEOUT) {
                        std::cerr << "Second timeout. Aborting request.\n"; // TODO: check if this valid
                        continue;
                    }
                }
                if (status == OK) {
                    std::cout << command << ": Player " << player_id << "'s game has been terminated.\n";
                    player_id.clear();
                    mystery_word.clear();
                } else if (status == NOK) {
                    std::cerr << command << ": No ongoing game for the given player ID.\n";
                } else if (status == ERR) {
                    std::cerr << command << ": Error.\n";
                } else if (status == UNEXPECTED_SYNTAX || status == UNEXPECTED_STATUS) {
                    std::cerr << command
                              << ": Invalid/unspecified player ID and/or unexpected message received from the server.\n";
                }
            } else std::cout << command << ": Game has been terminated.\n";
            if (command == "exit") {
                std::cout << "Program will exit.\n";
                break;
            }
        }
    }

    freeaddrinfo(res_udp);
    close(udp_socket_fd);

    return 0;
}