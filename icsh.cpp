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

//-------------------- Enumerators --------------------
enum class Command {ECHO, EXIT, NONE, EMPTY};
//enum class Redirect_IO {IN, OUT}; //IO append(IO) accordingly

//-------------------- Function foward declaration. --------------------
void populate_map_cmd(); //initial populating the string to command map. 
bool call_command(string); //call command switch.
Command sto_cmd(string); //change string to command. 
vector<string> tokenize_cmd(string&);//tokenize commands into parts <in case of handling flags too.>
void echo(vector<string>); //echo <text> 
bool double_bang_mod(string, string&); //<.*>!!<.*>
bool check_exit(vector<string> &); //check if exit is valid, set the prev_exit value. 
void script_mode(int , char **);//runs the script.
int external_cmd_call(vector<string>); //temporary external command caller this returns the exit val of child
void sigint_handler(int); //SIGINT handler
void sigtstp_handler(int); //SIGTSTP handler
bool IO_handle(vector<string>, int&); //handle redirection, 0: no redirect, 1: redirect
void do_redirect(vector<string>,int); //Handle IO redirection if there are pointers.
vector<string> tokenize_IO_FILE(vector<string>, int &);//Tokenize command to redirected files and status.

//-------------------- Global var init --------------------
std::map<std::string, Command> cmd_map;
string prev_cmd;
unsigned short prev_exit = 0;
pid_t foreground_pid = 0; //Temporary variable to store foreground process PID in Milestone 3
bool fg_run = 0; //Temporary variable to check if there is a foreground process is running.

//-------------------- Main Function and Setups --------------------

int main(int argc, char *argv[]){
    char buffer[MAX_CMD_BUFFER]; //maybe modify tokenizer to make it handle flags easier?
    bool exit = false;
    populate_map_cmd();

    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    
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
    string mod_inp;
    bool db_exist = double_bang_mod(inp, mod_inp); //double bang checking
    mod_inp.erase(remove(mod_inp.begin(), mod_inp.end(), '\n'), mod_inp.end());
    
    if(mod_inp.empty()) return false;
    if(db_exist) cout << mod_inp << endl;

    Command current = Command::EMPTY;    
    vector<string> base_command = tokenize_cmd(mod_inp); 
    
    if(!base_command.empty()) current = sto_cmd(base_command[0]);

    prev_cmd = mod_inp;

    switch(current){    
        case Command::ECHO:
            echo(base_command);
            cout << endl;
            prev_exit = 0;
            break;
        
        case Command::EXIT:
            exit = check_exit(base_command);
            if(!exit) cout << "bad command"<< endl; 
            else return exit; //this is to prevent the final endl on as opposed to other calls.
            break;
        
        case Command::EMPTY:
            cout << endl;
            return exit;

        case Command::NONE:
        default:
            prev_exit = external_cmd_call(base_command);
    }

    return exit;
}   

//-------------------- I/O Handling --------------------

bool IO_handle(vector<string> cmd, int& redir_i){
    int status;
    vector<string> cmd_IO_redir = tokenize_IO_FILE(cmd, status);

    if(status == 0) return 0;
    else do_redirect(cmd_IO_redir,status);
    return 1;
}

void do_redirect(vector<string> argv,int status){
    cout << argv[0].c_str() << endl;
    cout << argv[1].c_str() << endl;
    int in = open(argv[0].c_str(),O_RDONLY); //read to something
    int out = open(argv[1].c_str(), O_TRUNC|O_CREAT|O_WRONLY,0666); // write to something

    if((in <= 0) || (out <= 0)){
        fprintf(stderr,"Could not open a file.\n");
        exit (errno);
    }

    dup2(in,1);
    dup2(out, 0);

    close(in);
    close(out);
}
 
vector<string> tokenize_IO_FILE(vector<string> cmd, int &status){ 
    //Choose the first redirection sign only.
    //int status = 0; //0: No Redirect, 1: '<', 2: '>', 4: invalid input <deprecated>
    vector<string> cmd_IO;
    string f = "", s = "";
    status = 0;

    for (string c: cmd){
        if (c == "<" && !status){
            status = 1;

        } else if (c == ">" && !status){
            status = 2;
        } else {
            if (!status) f.append(c).append(" ");
            else s.append(c).append(" ");
        }
    }
    if(f.length()!= 0) f.pop_back();
    if(s.length()!= 0) s.pop_back();

    cmd_IO.push_back(f);
    cmd_IO.push_back(s);

    return cmd_IO;
}

//-------------------- Signal handling --------------------

void sigint_handler(int sig){
    if(fg_run) kill(foreground_pid,SIGINT);
}

void sigtstp_handler(int sig){
    if(fg_run) kill(foreground_pid,SIGTSTP);
}


//-------------------- Process handling --------------------

int external_cmd_call(vector<string> cmd){
    vector<char*> argv(cmd.size() +1);
    int status, n_exit_stat = EXIT_FAILURE;
    for(unsigned int i = 0; i < cmd.size(); i++){
        argv[i] = const_cast<char*>(cmd[i].c_str());
    }   

    foreground_pid = fork();
    
    if(foreground_pid < 0){
        perror("Fork failed.");
        exit(errno);
    } else if (foreground_pid == 0){
        IO_handle(cmd);

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

        //TODO: maybe required truncation with bitwise? (switch to unsigned char required?), 
        //handle error (find alternative to stoi that is not the very not so nice naive loop.) 
        //set default value required or not? maybe throw bad command? < else if(cmd.size() == 1) prev_exit = 0; >
        //or maybe find alternative to JMP to default case.
    } 

    return exit;
}

//-------------------- ENUM setups and CMD tokenization --------------------

void populate_map_cmd(){
    cmd_map["echo"] = Command::ECHO;
    cmd_map["exit"] = Command::EXIT;
}

vector<string> tokenize_cmd(string& inp){
    vector<string> separated;
    string next_str;
    istringstream cmd_stream(inp);
    
    while(getline(cmd_stream >> ws, next_str, ' ')){
        if (next_str != " ") separated.push_back(next_str);
    }
    return separated;
}

Command sto_cmd(string inp){
    return (cmd_map.find(inp) != cmd_map.end())? cmd_map[inp] : Command::NONE; 
}