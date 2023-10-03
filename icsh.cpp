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
struct Job{
    int status;
    pid_t pgid; //same as leader
    int job_id;
    bool is_alive;
    bool checked;
    string cmd;
    vector<pid_t> pid_list;
} ;

//maybe struct a new class for pid to point to job

//-------------------- Enumerators --------------------
enum class Command {ECHO, EXIT, EXTERNAL, EMPTY, FG, BG, JOBS};

//-------------------- Function Forward Declaration. --------------------
void populate_map_cmd(); //initial populating the string to command map. 
bool call_command(string); //prepare commands and call command switch.
Command sto_cmd(string); //change string to command. 
vector<string> tokenize_cmd(string&, int&, int&);//tokenize commands using whitespace with io redirection support.
void echo(vector<string>); //echo <text>    
bool double_bang_mod(string, string&); //<.*>!!<.*>
bool check_exit(vector<string> &); //check if exit is valid, set the prev_exit value. 
void script_mode(int , char **);//runs the script.
int external_cmd_call(vector<string>, bool, string); //temporary external command caller this returns the exit val of child
bool IO_handle(string, int, bool); //handle IO redirection main, 0: failure, 1: success
bool split_cmd_IO(vector<string> , int , int , vector<string>& , string&); //Split original command into new_command and filename
void backup_stdio(); //backup file descriptor of the original STDIO
void restore_stdio(); //restore to STDIO

int to_foreground(struct Job);
bool cont_background(struct Job);
bool jobs_list();
void update_jobs_list(); //TODO: Update this and make SIGCHLD to reap dead childs.
string status_mapping(int);

bool fgbg_cmd_get_job(string, struct Job &); // 0: not found, terminated; 1: found, the job input returns the current job.

//-------------------- Signal-Related FN Forward Declaration --------------------
void sigint_handler(int); //SIGINT handler
void sigtstp_handler(int); //SIGTSTP handler
//TODO: Handle zombie process and child reaping.

//-------------------- Global var init --------------------
std::map<std::string, Command> g_cmd_map;
string g_prev_cmd;
unsigned short g_prev_exit = 0;
pid_t g_pid = 0; //Temporary variable to store foreground process PID in Milestone 3
pid_t g_fg_pgid = 0; 
bool g_fg_run = 0; //Temporary variable to check if there is a foreground process is running.
int STDOUT_FDESC; //Stores file descriptor for stdout
int STDIN_FDESC; //Stores file descriptor for stdin

int g_shell_pgid = 0;
int g_cur_job_id_count = 0; //we start with 0 but the first job starts with 1 like this ++counter;
int g_job_unalive_count = 0;

vector<struct Job> g_bg_jobs; //background group process/ jobs 


//-------------------- Main Function and Setups --------------------

int main(int argc, char *argv[]){
    char buffer[MAX_CMD_BUFFER]; //maybe modify tokenizer to make it handle flags easier?
    bool exit = false;
    populate_map_cmd();

    g_shell_pgid = getpgid(0); //get the pid of the shell and store it beforehand.
    cout << g_shell_pgid << endl; //debug

    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    backup_stdio();
    
    if (argc > 1) {
        script_mode(argc, argv);
    } else { 
        cout << "Starting IC shell"<<endl;
        while (!exit) {
            cout << g_cur_job_id_count;
            update_jobs_list(); //update joblist before every prompt.
            printf("icsh $ ");
            string inp = fgets(buffer, 255, stdin); 
            exit = call_command(inp);
        }
    }

    return g_prev_exit;
}

//-------------------- Command calls --------------------

Command build_command(string inp, string& modded_input, vector<string>& cmd, bool& run_fg){
    //initializing variables
    int redir_index = -1,redir_type = -1; //redir index, type
    bool redir, db_exist, run_fg_tmp; 
    string tmp_mod_inp, filename;
    vector<string> new_cmd, full_cmd; 
    Command main_command = Command::EMPTY; 

    //Initial Clean-up
    db_exist = double_bang_mod(inp, tmp_mod_inp); //Check Double Bang
    tmp_mod_inp.erase(remove(tmp_mod_inp.begin(), tmp_mod_inp.end(), '\n'), tmp_mod_inp.end());// Erase EOL
    if(tmp_mod_inp.empty()) return Command::EMPTY; //force return Command::EMPTY if command is empty
    if(db_exist) cout << tmp_mod_inp << endl; //Repeat prompt if !! exist

    // Tokenize command
    full_cmd = tokenize_cmd(tmp_mod_inp,redir_index,redir_type);
    if(full_cmd.empty()) return Command::EMPTY;
    g_prev_cmd = tmp_mod_inp; //Set previous command history before & sign is removed.

    // Foreground Background checking
    run_fg_tmp = (!full_cmd.empty() && full_cmd.back() == "&")? 0:1; //run command in fg check
    if(!run_fg_tmp){
        full_cmd.pop_back();// remove & if exist, cmd set as bg
        tmp_mod_inp.pop_back();// remove & from original command.
    }

    // Get command ENUM
    if(!full_cmd.empty()) main_command = sto_cmd(full_cmd[0]); 
    
    // Redirection checking
    redir = split_cmd_IO(full_cmd, redir_index, redir_type, new_cmd, filename);
    if(redir) if(IO_handle(filename,redir_type,1)) full_cmd = new_cmd;   

    modded_input = tmp_mod_inp;
    cmd = full_cmd;
    run_fg = run_fg_tmp;

    return main_command;
}

bool call_command(string inp){
    bool exit = false, run_fg = false;
    string mod_inp;
    vector<string> full_cmd;
    Command main_cmd = build_command(inp, mod_inp, full_cmd, run_fg);
    struct Job job_of_interest;

    switch(main_cmd){    
        case Command::ECHO: 
            echo(full_cmd);
            g_prev_exit = 0;
            break;
        
        case Command::EXIT:
            exit = check_exit(full_cmd);
            if(!exit) cout << "bad command"<< endl; 
            else return exit; //this is to prevent the final endl on as opposed to other calls.
            break;
        
        case Command::EMPTY:
            break;

        case Command::FG:
            if(full_cmd.size() != 2 ){
                cout << "Invalid number of arguments!" << endl;   
                break;
            }
            if(fgbg_cmd_get_job(full_cmd[1],job_of_interest)) to_foreground(job_of_interest);
            else cout << "Invalid job id." << endl;

            break;

        case Command::BG:
            if(full_cmd.size() != 2 ){
                cout << "Invalid number of arguments!" << endl;   
                break;
            }
            if(fgbg_cmd_get_job(full_cmd[1],job_of_interest))cont_background(job_of_interest);
            else cout << "Invalid job id." << endl;
            break;

        case Command::JOBS:
            jobs_list();
            break;

        case Command::EXTERNAL:
            g_prev_exit = external_cmd_call(full_cmd,run_fg,mod_inp);
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
    if(g_fg_run && g_fg_pgid != -1) killpg(g_fg_pgid,SIGINT);
}

void sigtstp_handler(int sig){
    if(g_fg_run && g_fg_pgid != -1) killpg(g_fg_pgid,SIGTSTP);
}

void sigchld_handler(int sig){

}
//-------------------- Jobs control --------------------
void job_wait(){
    
}

int to_foreground(struct Job current_job){ //this must be called iff fgbg_cmd_get_job returns true or when spawining new process. Make handler to check if sigcont is necessary based on where its called. 
    int status, n_exit_stat = EXIT_FAILURE;
    pid_t pgid = current_job.pgid;
    tcsetpgrp(0,pgid);
    g_fg_pgid = pgid;

    if(killpg(pgid, SIGCONT) < 0){ //SIGCONT check here
        perror("SIGCONT FAILED"); //this is just a placeholder. 
    } 

    waitpid(pgid, &status, WUNTRACED);
        
    if(WIFEXITED(status)) n_exit_stat = WEXITSTATUS(status); 
    else if(WIFSTOPPED(status)){
        cout << "\nthe process has been stopped" << endl;
        n_exit_stat = 128 + WSTOPSIG(status); // 148 is signal for SIGTSTP
        
        if(current_job.job_id == 0) current_job.job_id = ++g_cur_job_id_count; //Put this in the background with the new handler
        current_job.status = 2; //placeholder for stopped.
        g_bg_jobs.push_back(current_job);
    }
    else if(WIFCONTINUED(status)){
        cout << "\nprocess" << pgid << "has continued" << endl;
        current_job.status = 1; //placeholder for stopped.
    }
    else if(WIFSIGNALED(status)){
        n_exit_stat = 128 + WTERMSIG(status); // 130 is signal for SIGINT
        cout << "\nprocess " << pgid << " has been terminated." << endl;
    }

    g_fg_pgid = -1;
    tcsetpgrp (0, g_shell_pgid);
    return n_exit_stat % 256;
}

bool cont_background(struct Job current_job){
    if(killpg(current_job.pgid, SIGCONT) < 0){
        perror("SIGCONT FAILED"); //this is just a placeholder.
        return 0;
    } 
    g_bg_jobs.push_back(current_job);
    return 1;
}

void update_jobs_list(){
    //call this at before each terminal loop start. Use queue to queue up the dead process string.
    for(struct Job j: g_bg_jobs){
        if (!j.checked && !j.is_alive){
            g_job_unalive_count++;
            j.checked = 1;
            cout << "[" << j.job_id << "] " << status_mapping(j.status) << "\t\t" << j.cmd << endl;
        } 
    }

    if(g_job_unalive_count == g_cur_job_id_count){ // do this last, we reset the job count to 0
        g_bg_jobs.clear(); //reset all job list to empty list, takes O(n) Amortized O(1) per each bg process ever created.
        g_job_unalive_count = 0;
        g_cur_job_id_count = 0;
    } 
}

string status_mapping(int status){ //maybe make a map for this.
    string stat_str;
    switch(status){
        default:
            stat_str = "Running";
    }
    return stat_str;        
}

bool jobs_list(){
    for(struct Job j : g_bg_jobs){
        cout << "[" << j.job_id << "] " << status_mapping(j.status) << "\t\t" << j.cmd << endl;
    }
    return 0;
}

bool fgbg_cmd_get_job(string inp, struct Job& j){
    //Check if the input is valid
    if(inp.size() < 2) return 0;
    if(inp[0] != '%') return 0;
    inp.erase(0,1);
    for(char c: inp) if(!isdigit(c)) return 0;

    int req_id = stoi(inp);
    for(struct Job cur_j : g_bg_jobs){
        if(cur_j.job_id == req_id){
            j = cur_j;
            return 1;
        } 
    }
    return 0;
}

//-------------------- Process handling --------------------

int external_cmd_call(vector<string> cmd, bool is_fg, string cmd_string){
    int n_exit_stat = EXIT_FAILURE; //status; //unused rn
    vector<string> new_cmd;
    string filename;
    
    vector<char*> argv(cmd.size() +1);
    for(unsigned int i = 0; i < cmd.size(); i++){
        argv[i] = const_cast<char*>(cmd[i].c_str());
    }   
    struct Job current_job;
    current_job.cmd = cmd_string;
    current_job.is_alive = 1;

    pid_t pid = fork();

    //NOTE: If we're gonna handle several process per each job, throw all this in loop while doing the following.
    //Determine which process will be the leader, maybe handle pipe from one process to another.

    if(pid < 0){
        perror("Fork failed.");
        exit(errno);
    } else if (pid == 0){
        execvp(argv[0],argv.data());
        perror("execvp call failed.");  
        exit(errno);
    } else {
        current_job.pid_list.push_back(pid);
        current_job.pgid = pid; //since we are going to have 1 process per job as for now.
        setpgid(pid,current_job.pgid);
         //TODO: make this increase only for background process and also start at 1.
        
        if(is_fg){
            g_fg_run = true; 
            current_job.job_id = 0;
            n_exit_stat = to_foreground(current_job);
        } else {
            current_job.job_id = ++g_cur_job_id_count;
            cont_background(current_job); 
        }

    }
    g_fg_run = false; 
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
        if(!g_prev_cmd.empty()) new_cmd = regex_replace(inp, regex("!!"),g_prev_cmd);
        return 1; 
    }
    new_cmd = inp;
    return 0;
}

void echo(vector<string> cmd){
    if (cmd.size() == 2 && cmd[1] == "$?") {
        cout << g_prev_exit;
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
        g_prev_exit = stoi(cmd[1]) %256; 

    } 

    return exit;
}

//-------------------- ENUM setups and CMD tokenization --------------------

void populate_map_cmd(){
    g_cmd_map["echo"] = Command::ECHO;    
    g_cmd_map["exit"] = Command::EXIT;
    g_cmd_map["fg"] = Command::FG;
    g_cmd_map["bg"] = Command::BG;
    g_cmd_map["jobs"] = Command::JOBS;
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
    return (g_cmd_map.find(inp) != g_cmd_map.end())? g_cmd_map[inp] : Command::EXTERNAL; 
}