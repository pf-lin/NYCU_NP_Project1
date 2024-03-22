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

// Use number pipe to classify cmd
vector<string> splitCommand(const string& command) {
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
    vector<string> cmdList;
    int splitIndex = 0;
    for (int i = 0; i < cmdSplitList.size(); i++) {
        if ((cmdSplitList[i][0] == '|' || cmdSplitList[i][0] == '!') && cmdSplitList[i].size() > 1) {
            string tmp = "";
            while (splitIndex < cmdSplitList.size() - 1) {
                tmp += cmdSplitList[splitIndex] + " ";
                splitIndex++;
            }
            tmp += cmdSplitList[splitIndex];
            cmdList.push_back(tmp);
            splitIndex++;
        }
    }
    if (splitIndex < cmdSplitList.size()) {
        string tmp = "";
        while (splitIndex < cmdSplitList.size() - 1) {
            tmp += cmdSplitList[splitIndex] + " ";
            splitIndex++;
        }
        tmp += cmdSplitList[splitIndex];
        cmdList.push_back(tmp);
    }

    return cmdList;
}

vector<string> parseCommand(const string& command) {
    vector<string> cmd;
    string token;
    stringstream ss(command);
    while (ss >> token) {
        cmd.push_back(token);
    }
    ss.clear();
    ss.str("");
    
    return cmd;
}

void build_in_command(const string& command) {
    vector<string> cmd = parseCommand(command);
    if (cmd[0] == "setenv") {
        setenv(cmd[1].c_str(), cmd[2].c_str(), 1);
    }
    else if (cmd[0] == "printenv") {
        const char* env = getenv(cmd[1].c_str());
        if (env != NULL) {
            cout << env << endl;
        }
    }
    else if (cmd[0] == "exit") {
        exit(0);
    }
}

// Function to execute a command
void executeCommand(const vector<string>& commands) {
    // TODO: Implement command execution logic here
    for (int i = 0; i < commands.size(); i++) {
        build_in_command(commands[i]);
    }
}

// Function to handle ordinary pipe
void handleOrdinaryPipe(const string& command1, const string& command2) {
    // TODO: Implement ordinary pipe logic here
}

// Function to handle numbered pipe
void handleNumberedPipe(const string& command, int pipeNumber) {
    // TODO: Implement numbered pipe logic here
}

// Function to handle file redirection
void handleFileRedirection(const string& command, const string& fileName, bool append) {
    // TODO: Implement file redirection logic here
}

int main() {
    string input;
    vector<string> commands;

    setenv("PATH", "bin:.", 1); // initial PATH is bin/ and ./

    while (true) {
        cout << "% "; // Use "% " as the command line prompt.
        getline(cin, input);

        commands = splitCommand(input);
        // for (int i = 0; i < commands.size(); i++) {
        //     cout << commands[i] << endl;
        // }
        executeCommand(commands);
    }

    return 0;
}