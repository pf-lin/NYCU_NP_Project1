#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

using namespace std;

struct NumberedPipe {
    int pipeCmdId; // the command id that the numbered pipe is connected to
    int numPipefd[2];
};

struct CommandInfo {
    vector<string> cmdList;
    int cmdId;
};

struct Process {
    vector<string> args;
    bool isOrdinaryPipe = false;
    bool isNumberedPipe = false;
    bool isErrPipe = false;
    int pipeNumber;
    int *to = nullptr;
    int *from = nullptr;
};

int cmdCount = 0; // count the number of commands
vector<NumberedPipe> numPipeList; // store numbered pipe

// Use number pipe to classify cmd
vector<CommandInfo> splitCommand(const string& command) {
    // Split the command by ' ' (space)
    vector<string> cmdSplitList;
    string token;
    stringstream ss(command);
    while (ss >> token) {
        cmdSplitList.push_back(token);
    }
    ss.clear();
    ss.str("");

    // Combine cmdSplitList into a command
    vector<CommandInfo> cmdInfoList;
    int splitIndex = 0;
    for (int i = 0; i < cmdSplitList.size(); i++) {
        if ((cmdSplitList[i][0] == '|' || cmdSplitList[i][0] == '!') && cmdSplitList[i].size() > 1) {
            CommandInfo cmdInfo;
            cmdInfo.cmdId = cmdCount++;
            while (splitIndex <= i) {
                cmdInfo.cmdList.push_back(cmdSplitList[splitIndex]);
                splitIndex++;
            }
            cmdInfoList.push_back(cmdInfo);
        }
    }
    if (splitIndex < cmdSplitList.size()) {
        CommandInfo cmdInfo;
        cmdInfo.cmdId = cmdCount++;
        while (splitIndex < cmdSplitList.size()) {
            cmdInfo.cmdList.push_back(cmdSplitList[splitIndex]);
            splitIndex++;
        }
        cmdInfoList.push_back(cmdInfo);
    }

    return cmdInfoList;
}

// Parse command into each process (fork and execvp)
vector<Process> parseCommand(const CommandInfo& cmdInfo) {
    vector<Process> processList;
    int cmdListIndex = 0;
    for (int i = 0; i < cmdInfo.cmdList.size(); i++) {
        if ((cmdInfo.cmdList[i][0] == '|' || cmdInfo.cmdList[i][0] == '!') && cmdInfo.cmdList[i].size() > 1) {
            Process process;
            if (cmdInfo.cmdList[i][0] == '|') // numbered pipe
                process.isNumberedPipe = true;
            else // error pipe
                process.isErrPipe = true;

            process.pipeNumber = stoi(cmdInfo.cmdList[i].substr(1));
            while (cmdListIndex < i) {
                process.args.push_back(cmdInfo.cmdList[cmdListIndex]);
                cmdListIndex++;
            }
            cmdListIndex++;
            processList.push_back(process);
        }
        else if (cmdInfo.cmdList[i][0] == '|' && cmdInfo.cmdList[i].size() == 1) { // ordinary pipe
            Process process;
            process.isOrdinaryPipe = true;
            while (cmdListIndex < i) {
                process.args.push_back(cmdInfo.cmdList[cmdListIndex]);
                cmdListIndex++;
            }
            cmdListIndex++;
            processList.push_back(process);
        }
    }
    if (cmdListIndex < cmdInfo.cmdList.size()) {
        Process process;
        while (cmdListIndex < cmdInfo.cmdList.size()) {
            process.args.push_back(cmdInfo.cmdList[cmdListIndex]);
            cmdListIndex++;
        }
        processList.push_back(process);
    }
    return processList;
}

bool build_in_command(const CommandInfo& cmdInfo) {
    if (cmdInfo.cmdList[0] == "setenv") {
        // TODO: input validation (not in spec but better to have it) -- by newb1er
        if (cmdInfo.cmdList.size() != 3) {
            cerr << "Invalid number of arguments for setenv command." << endl;
            return false;
        }
        setenv(cmdInfo.cmdList[1].c_str(), cmdInfo.cmdList[2].c_str(), 1);
    }
    else if (cmdInfo.cmdList[0] == "printenv") {
        // TODO: input validation (not in spec but better to have it) -- by newb1er
        if (cmdInfo.cmdList.size() != 2) {
            cerr << "Invalid number of arguments for printenv command." << endl;
            return false;
        }
        const char* env = getenv(cmdInfo.cmdList[1].c_str());
        if (env != NULL) {
            cout << env << endl;
        }
    }
    else if (cmdInfo.cmdList[0] == "exit") {
        exit(0);
    }
    else {
        return false;
    }
    return true;
}

void execute(const Process& process) {
    char* argv[process.args.size() + 1];
    for (int i = 0; i < process.args.size(); i++) {
        if (process.args[i] == ">") { // file redirection
            int fd = open(process.args[i + 1].c_str(), O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
            dup2(fd, STDOUT_FILENO);
            close(fd);
            argv[i] = NULL;
            break;
        }
        argv[i] = (char*)process.args[i].c_str();
    }
    argv[process.args.size()] = NULL;

    if (execvp(argv[0], argv) == -1) {
        cerr << "Unknown command: [" << argv[0] << "]." << endl;
        exit(0);
    }
}

// Function to execute each command
void executeCommand(const vector<CommandInfo>& commands) {
    pid_t pid;
    for (int i = 0; i < commands.size(); i++) { // for each command
        if (build_in_command(commands[i])) {
            continue;
        }
        
        vector<Process> processList = parseCommand(commands[i]);

        // Find if there is a number pipe to pass to this command
        bool isNumPipeInput = false;
        int numPipeIndex;
        for (int np = 0; np < numPipeList.size(); np++) {
            if (numPipeList[np].pipeCmdId == commands[i].cmdId) {
                isNumPipeInput = true;
                numPipeIndex = np;
                break;
            }
        }

        int pipefd[2][2]; // pipefd[0] for odd process, pipefd[1] for even process
        for (int j = 0; j < processList.size(); j++) { // for each process
            if (j == 0 && isNumPipeInput/* There is a number pipe to write to*/) {
                processList[j].from = numPipeList[numPipeIndex].numPipefd; // read from number pipe
            }
            if (processList[j].isNumberedPipe || processList[j].isErrPipe) { // create numbered pipe
                int numPipeCmdId = commands[i].cmdId + processList[j].pipeNumber;

                // check if a numbered pipe connected to the same command has been established
                for (int np = 0; np < numPipeList.size(); np++) {
                    if (numPipeList[np].pipeCmdId == numPipeCmdId) {
                        processList[j].to = numPipeList[np].numPipefd;
                        break;
                    }
                }

                // if not, create a new numbered pipe
                if (processList[j].to == nullptr) {
                    NumberedPipe numPipe;
                    numPipe.pipeCmdId = numPipeCmdId;
                    pipe(numPipe.numPipefd);
                    numPipeList.push_back(numPipe);
                    processList[j].to = numPipeList[numPipeList.size() - 1].numPipefd;
                }
            }
            if (j > 0) {
                processList[j].from = pipefd[(j - 1) % 2];
            }
            if (j < processList.size() - 1) {
                processList[j].to = pipefd[j % 2];
                pipe(pipefd[j % 2]);
            }

            pid = fork();
            if (pid == 0) { // child process
                if (processList[j].to != nullptr) {
                    close(processList[j].to[0]);
                    dup2(processList[j].to[1], STDOUT_FILENO);
                    if (processList[j].isErrPipe) {
                        dup2(processList[j].to[1], STDERR_FILENO);
                    }
                    close(processList[j].to[1]);
                }
                if (processList[j].from != nullptr) {
                    close(processList[j].from[1]);
                    dup2(processList[j].from[0], STDIN_FILENO);
                    close(processList[j].from[0]);
                }
                execute(processList[j]);
            }
            else { // parent process
                if (j == 0 && isNumPipeInput) { // close number pipe
                    close(numPipeList[numPipeIndex].numPipefd[0]);
                    close(numPipeList[numPipeIndex].numPipefd[1]);
                }
                if (j > 0) { // close pipe
                    close(pipefd[(j - 1) % 2][0]);
                    close(pipefd[(j - 1) % 2][1]);
                }
                if (j == processList.size() - 1) {
                    waitpid(pid, NULL, 0);
                }
            }
        }
    }
}

int main() {
    string input;
    vector<CommandInfo> commands;

    setenv("PATH", "bin:.", 1); // initial PATH is bin/ and ./

    while (true) {
        cout << "% "; // Use "% " as the command line prompt.
        getline(cin, input);

        commands = splitCommand(input);
        executeCommand(commands);
    }

    return 0;
}