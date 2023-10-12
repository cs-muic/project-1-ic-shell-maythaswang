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

# Manual
---------
In this section we will discuss the syntaxes and implementations of each commands and functionalities. 

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

case 2: the command echo will print the most recent exit status of the terminated foreground process or built-in commands to the console. *note: echo will behave in this manner iff the only text following the command `echo` is `$?`*

Example: 
```shell
echo $?
↪ 130
```

### exit(1)
--- 
**NAME**
`exit` - exit the shell with the given exit status

**SYNOPSIS**
```shell
exit <num>
```

**DESCRIPTION**
The shell exits and the return value is set to `<num>`