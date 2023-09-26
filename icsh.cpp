/* ICCS227: Project 1: icsh
 * Name: Maythas Wangcharoenwong
 * StudentID: 6480265
 */

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <regex>
#include <fstream>

#define MAX_CMD_BUFFER 255

using namespace std;

//Enumerators
enum class Command {ECHO, EXIT, NONE, EMPTY};

//Function foward declaration.
void populate_map_cmd(); //initial populating the string to command map. 
bool call_command(string); //call command switch.
Command sto_cmd(string); //change string to command. 
vector<string> tokenize_cmd(string);//tokenize commands into parts <in case of handling flags too.>
void echo(vector<string>); //echo <text> 
bool double_bang_mod(string, string&); //<.*>!!<.*>
bool check_exit(vector<string> &); //check if exit is valid, set the prev_exit value. 
void script_mode(int , char **);//runs the script.

//Global var init
std::map<std::string, Command> cmd_map;
string prev_cmd;
unsigned short prev_exit = 0;

int main(int argc, char *argv[]){
    char buffer[MAX_CMD_BUFFER]; //maybe modify tokenizer to make it handle flags easier?
    bool exit = false;
    populate_map_cmd();
    
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
    }

    //TODO: check if we need to repeat the function prompt first before calling when using !!
}

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

    switch(current){    
        case Command::ECHO:
            echo(base_command);
            prev_exit = 0;
            break;
        
        case Command::EXIT:
            exit = check_exit(base_command);
            if(!exit) cout << "bad command"; 
            else return exit; //this is to prevent the final endl on as opposed to other calls.
            break;
        
        case Command::EMPTY:
            return exit;

        case Command::NONE:
        default:
            cout << "bad command"; 
    }

    prev_cmd = mod_inp;
    cout << endl;

    return exit;
}

bool double_bang_mod(string inp, string &new_cmd){ //This is currently implemented in a similar manner to bash.
    if(inp.find("!!") != string::npos){
        if(!prev_cmd.empty()) new_cmd = regex_replace(inp, regex("!!"),prev_cmd);
        return 1; 
    }
    new_cmd = inp;
    return 0;
}

void echo(vector<string> cmd){
    if (cmd.size() > 1) {
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
        
        prev_exit = stoi(cmd[1]) %265; 

        //TODO: maybe required truncation with bitwise? (switch to unsigned char required?), 
        //handle error (find alternative to stoi that is not the very not so nice naive loop.) 
        //set default value required or not? maybe throw bad command? < else if(cmd.size() == 1) prev_exit = 0; >
        //or maybe find alternative to JMP to default case.
    } 

    return exit;
}

void populate_map_cmd(){
    cmd_map["echo"] = Command::ECHO;
    cmd_map["exit"] = Command::EXIT;
}

vector<string> tokenize_cmd(string inp){
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