Modified by: Maythas Wangcharoenwong 20231012
# ICSH - About the Shell
-------------
This ICSH (IC shell) was implemented as a part of MUIC's PCSA (Principle of System Architecture) course by Maythas Wangcharoenwong. The shell is mostly implemented in a similar manner to how bash would react to certain commands. However, we will be quite strict on syntaxes meaning that the input must strictly be in the same format as designed in order to make the shell perform as intended.
### What this shell supports: 
- Basic built-in commands
- Script mode
- Basic IO redirection
- Jobs control
- Using external program.
- Basic signal handling
### What this shell doesn't support:
- Piping `cmd_A | cmd_B | cmd_C`
- Weird I/O redirection `a < b < c > d > e > f`
- `rm -rf /` your Host device *given you're running this in a VM.*
- **A lot of stuffs.**
# Versions
-----------
### v0.1.0 - Built-in Commands & Basic Functionalities
- Built-in commands:
	- `echo <text>` : prints out the text to the console.
	- `!!` : repeat the previous input ***per every occurrence*** of "!!"
	- `exit <num>` : exit the shell with the given number.
- Command Tokenizer \<string to vector>
- Command Enumerator.
- Handling empty input.
- Handling bad input.
### v0.2.0 - Script Mode
- Implemented Script mode.
	- `./icsh <file>`
### v0.3.0 - Foreground External Program handling
- Implemented running external program on foreground.
### v0.4.0 - Handle signal SIGTSTP and SIGINT
- (Ctrl+Z) sends SIGTSTP to foreground process suspending and bringing them to background.
- (Ctrl+C) sends SIGINT to foreground process terminating the process.
- Built-in commands:
	- `echo $?` : print the exit status of the most recently terminated process.
- Handling exit value from child processes. 
### v0.5.0 - I/O redirection
- Implemented one-time I/O redirection per input for all commands.
	- `<command> > <file>` : redirect output of the command to file.
	- `<command> < <file>` : redirect input of command to the file.
### v0.6.0 - Job Controls
- Implemented foreground background jobs.
- Implemented terminal control switching.
- Handle SIGCHLD.
- Built-in commands:
	- `fg %<job_id>` : continue a stopped job in the background and bring them to foreground.
	- `bg %<job_id>` : continue a stopped job in the background.
	- `jobs` : shows the list and statuses of jobs. 
### v0.7.0 - Extra Features
- Built-in Commands:
	- `!!` : *note: this functionality is implemented right from v0.1.0*
	- `help` : shows basic command syntaxes.
	- `cd <dir>`: change directory.

# Implementation and Designs (High-Level Discussion)
----





# Manual
---------
In this section we will discuss the syntaxes and functionalities of each commands. 

The character ↪ will indicate the return value or value printed to console.
### echo(1)
---
**NAME**
`echo` - prints the message specified to the console.

**SYNOPSIS**
```shell
1. echo <input_text>
2. echo $?
```

**DESCRIPTION**
case 1:  the command echo will print the all the texts that comes after the command echo to the console. All this whitespaces in the start and end of the input will be dropped when printed out to the console. The consecutive whitespace characters at other positions will be truncated to at most 1 character of whitespace per gap.

Example:
```shell
echo Never   gonna       give           you       up
↪ Never gonna give you up
```

case 2: the command echo will print the most recent exit status of the terminated foreground process or built-in commands to the console. *note: echo will behave in this manner iff the only text following the command `echo` is `$?`*.

Example: 
```shell
echo $?
↪ 130
```

### exit(1)
--- 
**NAME**
`exit` - exit the shell with the given exit status.

**SYNOPSIS**
```shell
exit <num>
```

**DESCRIPTION**
The shell exits and the return value is set to `<num>`.

### cd(1)
------
**NAME**
`cd` - change the current directory to the specified directory.

**SYNOPSIS**
```shell
1. cd <dir>
2. cd ~
```

**DESCRIPTION**
case 1: The current directory is set to `<dir>` if the specified directory is accessible, otherwise and error will be thrown to the console. 

case 2: The current directory will be set to HOME.

### cd(0)
---- 
**NAME**
`cd` -  change the current directory to the specified directory.

**SYNOPSIS**
```shell
cd 
```

**DESCRIPTION**
The current directory will be set to HOME.

### jobs(0)
---- 
**NAME**
`jobs` - show the current jobs and their statuses list on the console.

**SYNOPSIS**
```shell
jobs
```

**DESCRIPTION**
Show the current jobs list and their statuses to the console in the format 
`[job_id] <status>           <command>`

if the job is running in the background, there will be `&` sign added to the back of the job.

```shell
[1] Running               sleep 2 &
[2] Done                  ./test
[3] Interrupted           ./a
[4] Stopped               sleep 50
[5] Killed                sleep 5
[6] EXIT 143              sleep 1

```

Each statuses have these meanings accordingly:
```
1. Running     : The job is currently running in the background. 
2. Done        : The job have terminated normally.
3. Interrupted : The job was terminated by SIGINT or (Ctrl+C)
4. Stopped     : The job is stopped by one of the stop signals including SIGTSTP. (Ctrl+Z)
5. Killed      : The job was terminated by the signal SIGKILL.
6. EXIT <num>  : The job is killed, stopped by the other signals not mentioned above.
```

**Jobs status behavior**
After any jobs has entered a finished state,  (any status apart from running and stopped.) After the shell has received the signal, by the next prompt, the job will stop showing in the job list when `jobs` is called.

### bg(1)
-----
**NAME**
`bg %<job_id>` - continue background job.

**SYNOPSIS**
```shell
bg %<job_id>
```

**DESCRIPTION**
This command will continue any jobs in the background with the corresponding `<job_id>`, it the job doesn't exits or has already finished, "Invalid job id" will be written to the console. If the background job is already running, do nothing.

### fg(1)
---- 
**NAME**
`fg %<job_id>` - bring background job to foreground and continue.

**SYNOPSIS**
```shell
fg %<job_id>
```

**DESCRIPTION**
This command will bring the background job to foreground according to its `<job_id>`, and continues the job. 


## Special Characters
### !!
-----
**NAME**
`!!` - double bang, repeats the previous input string. 

**SYNOPSIS**
```shell
# Assume the previous command is echo a
!!
↪ echo a
↪ a
!!!!
↪ echo aecho a
↪ aecho a
!!! 
↪ echo aecho a!
↪ aecho a!
```

**DESCRIPTION**
This command is implemented in the same manner as bash, using REGEX, every occurrence of `!!` will now be replaced by the previous input string. After the new input string is built, it is now sent to the command processing function to act accordingly.

## IO Redirection
### >
----
**NAME**
`<command> > <file>` - Redirecting command output to file.

**SYNOPSIS**
```shell
<command> > <file>
```

**DESCRIPTION**
This command allow only one time redirection per input, the first occurrence of `>` will be the splitting point between the command and the file.

Example: 
```shell
echo a > test.txt
```
Doing this will write a to the file test.txt

### < 
-----
**NAME**
`<command> < <file>` - Redirection input of the command to the file.

**SYNOPSIS**
```shell
<command> < <file> 
```

**DESCRIPTION**
This command allow only one time redirection per input, the first occurrence of `<` will be the splitting point between the command and the file.

Example: 
```shell
cat < test.txt
↪ a
```

### \<external command>
-----
**NAME**
`<external command>` - Call other external commands.

**SYNOPSIS**
```shell
<external command>
```

**DESCRIPTION**
Any input that does not match with the syntaxes prior to this will fall into this case. The command `<external command>` will be called after forking using execvp. The `<external command>` will perform accordingly the way it is programmed to if exist and the argument is passed correctly, otherwise, an error will be thrown to the console.

----
<center>END OF MANUAL</center>
