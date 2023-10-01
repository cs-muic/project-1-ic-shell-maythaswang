 /* ICCS227: Project 1: icsh
 * Name: Maythas Wangcharoenwong
 * StudentID: 6480265
 */

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <sstream>
#include <regex>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_CMD_BUFFER 255

using namespace std;

//-------------------- Structs --------------------
struct Jobs{
    int status;
    pid_t pgid; //same as leader
    pid_t job_id;
    vector<pid_t> pid_list;
} ;

//-------------------- Enumerators --------------------
enum class Command {ECHO, EXIT, EXTERNAL, EMPTY, FG, BG, JOBS};

//-------------------- Function foward declaration. --------------------
void populate_map_cmd(); //initial populating the string to command map. 
bool call_command(string); //prepare commands and call command switch.
Command sto_cmd(string); //change string to command. 
vector<string> tokenize_cmd(string&, int&, int&);//tokenize commands using whitespace with io redirection support.
void echo(vector<string>); //echo <text>    
bool double_bang_mod(string, string&); //<.*>!!<.*>
bool check_exit(vector<string> &); //check if exit is valid, set the prev_exit value. 
void script_mode(int , char **);//runs the script.
int external_cmd_call(vector<string>); //temporary external command caller this returns the exit val of child
void sigint_handler(int); //SIGINT handler
void sigtstp_handler(int); //SIGTSTP handler
bool IO_handle(string, int, bool); //handle IO redirection main, 0: failure, 1: success
bool split_cmd_IO(vector<string> , int , int , vector<string>& , string&); //Split original command into new_command and filename
void backup_stdio(); //backup file descriptor of the original STDIO
void restore_stdio(); //restore to STDIO

bool to_foreground(pid_t pgid);
bool to_background(pid_t pgid);
bool jobs_list();

//-------------------- Global var init --------------------
std::map<std::string, Command> cmd_map;
string prev_cmd;
unsigned short prev_exit = 0;
pid_t foreground_pid = 0; //Temporary variable to store foreground process PID in Milestone 3
bool fg_run = 0; //Temporary variable to check if there is a foreground process is running.
int STDOUT_FDESC; //Stores file descriptor for stdout
int STDIN_FDESC; //Stores file descriptor for stdin

vector<struct Jobs> g_bg_groups; //background groups jobs <or maybe change this to gpid/ job_id ?>


//-------------------- Main Function and Setups --------------------

int main(int argc, char *argv[]){
    char buffer[MAX_CMD_BUFFER]; //maybe modify tokenizer to make it handle flags easier?
    bool exit = false;
    populate_map_cmd();

    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    backup_stdio();
    
    if (argc > 1) {
        script_mode(argc, argv);
    } else { 
        cout << "Starting IC shell"<<endl;
        while (!exit) {
            printf("icsh $ ");
            string inp = fgets(buffer, 255, stdin); 
            exit = call_command(inp);
        }
    }

    return prev_exit;
}

//-------------------- Command calls --------------------

bool call_command(string inp){
    bool exit = false;
    int redir_index = -1,redir_type = -1;
    string mod_inp;

    bool db_exist = double_bang_mod(inp, mod_inp); //Check Double Bang
    mod_inp.erase(remove(mod_inp.begin(), mod_inp.end(), '\n'), mod_inp.end());// Erase EOL
    
    if(mod_inp.empty()) return false; //Re-prompt if empty command
    if(db_exist) cout << mod_inp << endl; //Repeat prompt if !! exist

    Command current = Command::EMPTY; //Set default enum to empty
    vector<string> base_command = tokenize_cmd(mod_inp,redir_index,redir_type);  //tokenize command

    if(!base_command.empty()) current = sto_cmd(base_command[0]); //Get command ENUM
    prev_cmd = mod_inp;

    vector<string> new_cmd;
    string filename;
    bool redir = split_cmd_IO(base_command, redir_index, redir_type, new_cmd, filename);;
    if(redir){
        if(IO_handle(filename,redir_type,1)) base_command = new_cmd;   
    }

    switch(current){    
        case Command::ECHO: //Isolate io redirect only to echo.
            echo(base_command);
            prev_exit = 0;
            break;
        
        case Command::EXIT:
            exit = check_exit(base_command);
            if(!exit) cout << "bad command"<< endl; 
            else return exit; //this is to prevent the final endl on as opposed to other calls.
            break;
        
        case Command::EMPTY:
            cout << endl;
            break;

        case Command::FG:
            cout << "fg called!" << endl;
            break;

        case Command::BG:
            cout << "bg called!" << endl;
            break;

        case Command::JOBS:
            cout << "jobs called!" << endl;
            break;

        case Command::EXTERNAL:
        default:
            prev_exit = external_cmd_call(base_command);
    }
    restore_stdio();
    return exit;
}   

//-------------------- I/O Handling --------------------

bool IO_handle(string filename, int redir_type, bool internal){
    int in,out; 
    if(redir_type == 0){ // OUT > 
        in = open(filename.c_str(), O_TRUNC|O_CREAT|O_WRONLY,0666); 
        
        if(in <= 0){
            fprintf(stderr,"Could not open a file.\n");
            if(!internal) exit(errno); //try set exit only for child processes.
            else return 0;
        }
        dup2(in,1);
        close(in);
    } else if (redir_type == 1){ // < IN
        out = open(filename.c_str(),O_RDONLY); 
        
        if(out <= 0){
            fprintf(stderr,"Could not open a file.\n");
            if(!internal) exit(errno);
            else return 0;
        }
        dup2(out, 0);
        close(out);
    } else return 0;
    return 1;
}

/**
 * @brief splits the command into new_cmd and filename, the function returns whether IO redirection is required.
 * @return bool (0: no redir, 1: has redir)

 */
bool split_cmd_IO(vector<string> cmd, int redir_i, int redir_type, vector<string>& new_cmd, string& filename){ 
    filename = "";
    if(redir_type == -1) return 0;
    else{
        for(int i = 0; i < redir_i; i++) new_cmd.push_back(cmd[i]);
        for(unsigned int i = redir_i + 1; i < cmd.size(); i++) filename.append(cmd[i]).append(" ");
    }
    if(!filename.empty()) filename.pop_back();
    return 1;
}

void backup_stdio(){
    STDOUT_FDESC = dup(0);
    STDIN_FDESC = dup(1);
}

void restore_stdio(){
    dup2(STDOUT_FDESC,0);
    dup2(STDIN_FDESC,1);
}
//-------------------- Signal handling --------------------

void sigint_handler(int sig){
    if(fg_run) kill(foreground_pid,SIGINT);
}

void sigtstp_handler(int sig){
    if(fg_run) kill(foreground_pid,SIGTSTP);
}

//-------------------- Jobs control --------------------
bool to_foreground(pid_t pgid){
    return 0;
}

bool to_background(pid_t pgid){
    kill(pgid, SIGCONT);
    return 0;
}

bool jobs_list(){
    for(struct Jobs j : g_bg_groups){
        printf("yay!");
    }
    return 0;
}
//-------------------- Process handling --------------------

int external_cmd_call(vector<string> cmd){
    int status, n_exit_stat = EXIT_FAILURE;
    vector<string> new_cmd;
    string filename;
    
    vector<char*> argv(cmd.size() +1);
    for(unsigned int i = 0; i < cmd.size(); i++){
        argv[i] = const_cast<char*>(cmd[i].c_str());
    }   

    foreground_pid = fork();
    
    if(foreground_pid < 0){
        perror("Fork failed.");
        exit(errno);
    } else if (foreground_pid == 0){
        execvp(argv[0],argv.data());
        fg_run = true; 
        perror("execvp call failed.");  
        exit(errno);
    } else {
        waitpid(foreground_pid, &status, WUNTRACED);
        if(WIFEXITED(status)) n_exit_stat = WEXITSTATUS(status); 
        else if(WTERMSIG(status)){
            n_exit_stat = 1; 
            cout << "\nprocess " << foreground_pid << " has been terminated." << endl;
        }
        else if(WIFSTOPPED(status)){
            cout << "\nthe process has been stopped" << endl;
        }
    }
    fg_run = false; 
    return n_exit_stat %256;
}

//-------------------- Script mode --------------------

void script_mode(int argc, char *argv[]){ 
    if(argc != 2) cout << "Invalid numbers of arguments." << endl;
    else {
        string buffer;//[MAX_CMD_BUFFER];

        ifstream inp_script(argv[1]);

        if(inp_script.is_open()){
            while(getline(inp_script, buffer)){
                call_command(buffer);
            }
        }
        else cout << "Invalid arguments." << endl;

        inp_script.close();
    }  //TODO: check if we need to repeat the function prompt first before calling when using !!

}

//-------------------- Built-in commands --------------------

bool double_bang_mod(string inp, string &new_cmd){ //This is currently implemented in a similar manner to bash.
    if(inp.find("!!") != string::npos){
        if(!prev_cmd.empty()) new_cmd = regex_replace(inp, regex("!!"),prev_cmd);
        return 1; 
    }
    new_cmd = inp;
    return 0;
}

void echo(vector<string> cmd){
    if (cmd.size() == 2 && cmd[1] == "$?") {
        cout << prev_exit;
    }
    else if (cmd.size() > 1) {
        for (unsigned int i = 1; i < cmd.size(); i++){ cout << cmd[i] << " ";}
    }
    cout << endl;
}

bool check_exit(vector<string> &cmd){ // returns exit bool
    bool exit = false, all_digit = true;
    if(cmd.size() == 2) { 
        for (char c: cmd[1]){
            if(!isdigit(c)){all_digit = false; break;}
        }   
        if(!all_digit) return exit;

        exit = true;
        prev_exit = stoi(cmd[1]) %256; 

    } 

    return exit;
}

//-------------------- ENUM setups and CMD tokenization --------------------

void populate_map_cmd(){
    cmd_map["echo"] = Command::ECHO;    
    cmd_map["exit"] = Command::EXIT;
    cmd_map["fg"] = Command::FG;
    cmd_map["bg"] = Command::BG;
    cmd_map["jobs"] = Command::JOBS;
}

vector<string> tokenize_cmd(string& inp, int& redir_index, int& redir_type){ //implement handling for IO redirection support here.
    vector<string> separated;
    string next_str;
    istringstream cmd_stream(inp);
    int i = 0;

    while(getline(cmd_stream >> ws, next_str, ' ')){
        if (next_str != " "){
            separated.push_back(next_str);
            if(next_str == ">" && redir_index == -1) { //OUT
                redir_type = 0; 
                redir_index = i;
            } else if(next_str == "<" && redir_index == -1){ //IN
                redir_type = 1;
                redir_index = i;
            }
            i++;
        }
        
    }
    return separated;
}

Command sto_cmd(string inp){
    return (cmd_map.find(inp) != cmd_map.end())? cmd_map[inp] : Command::EXTERNAL; 
}