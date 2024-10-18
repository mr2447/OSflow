#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstring>
using namespace std;

const string ERROR = "Syntax of flow file incorrect";

struct Node {
    string name;
    string command;
};

struct Pipe {
    string name;
    string from;
    string to;
};

struct Concatenate {
    string name;
    int parts;
    vector<string> part_names;
};

struct Flow {
    unordered_map<string, Node> nodes;         // Map of nodes
    unordered_map<string, Pipe> pipes;         // Map of pipes
    unordered_map<string, Concatenate> concats; // Map of concatenates
};

// Function prototypes
void executeNode(const Node& node);
void executePipe(Flow& flow, const Pipe& pipe);
void executeConcatenate(const Concatenate& concat, Flow& flow);

// Function to read the flow file and populate the Flow structure
int readFile(const string& fileName, Flow& flow) {
    ifstream file(fileName);

    if (!file.is_open()) {
        return 1; // File could not be opened
    }

    Node currNode;
    Pipe currPipe;
    Concatenate currCat;

    string line;
    while (getline(file, line)) {
        if (line.compare(0, 5, "node=") == 0) {
            currNode.name = line.substr(5);
        } else if (line.compare(0, 8, "command=") == 0) {
            currNode.command = line.substr(8);
            flow.nodes[currNode.name] = currNode; // Store in the unordered_map
        } else if (line.compare(0, 5, "pipe=") == 0) {
            currPipe.name = line.substr(5);
        } else if (line.compare(0, 5, "from=") == 0) {
            currPipe.from = line.substr(5);
        } else if (line.compare(0, 3, "to=") == 0) {
            currPipe.to = line.substr(3);
            flow.pipes[currPipe.name] = currPipe; // Store in the unordered_map
        } else if (line.compare(0, 12, "concatenate=") == 0) {
            currCat.name = line.substr(12);
        } else if (line.compare(0, 6, "parts=") == 0) {
            currCat.parts = stoi(line.substr(6));
            currCat.part_names.resize(currCat.parts);
        } else if (line.compare(0, 5, "part_") == 0) {
            int idx = stoi(line.substr(5, 1));
            currCat.part_names[idx] = line.substr(line.find('=') + 1);
            if (idx + 1 == currCat.part_names.size()) {
                flow.concats[currCat.name] = currCat; // Store in the unordered_map
            }
        }
    }

    file.close();
    return 0;
}

void executeConcatenate(const Concatenate& concat, Flow& flow) {
    for (const auto& partName : concat.part_names) {
        if (flow.nodes.find(partName) != flow.nodes.end()) {
            executeNode(flow.nodes[partName]);
        } else if (flow.pipes.find(partName) != flow.pipes.end()) {
            executePipe(flow, flow.pipes[partName]);
        }
    }
}

void executeNode(const Node& node) {
    pid_t pid = fork();

    if (pid == 0) { // Child process
        char* args[100];
        string command = node.command;
        istringstream iss(command);
        string arg;
        int i = 0;

        while (iss >> arg) {
            if (arg.front() == '\'' && arg.back() == '\'') {
                arg = arg.substr(1, arg.size() - 2);
            }
            args[i] = new char[arg.size() + 1];
            strcpy(args[i], arg.c_str());
            i++;
        }
        args[i] = nullptr;

        if (execvp(args[0], args) < 0) {
            perror("execvp failed");
            exit(1);
        }
    } else if (pid < 0) {
        perror("fork failed");
    } else {
        waitpid(pid, nullptr, 0); // Parent process waits for child
    }
}

void executePipe(Flow& flow, const Pipe& pipe) {
    pid_t p1 = fork();

    if (p1 == 0) {
        int fd[2];
        ::pipe(fd);

        pid_t p2 = fork();
        if (p2 == 0) {
            dup2(fd[1], STDOUT_FILENO); // Redirect stdout to pipe
            close(fd[0]);
            close(fd[1]);

            if (flow.nodes.find(pipe.from) != flow.nodes.end()) {
                executeNode(flow.nodes[pipe.from]);
            } else if (flow.concats.find(pipe.from) != flow.concats.end()) {
                executeConcatenate(flow.concats[pipe.from], flow);
            }
            exit(0);
        }

        pid_t p3 = fork();
        if (p3 == 0) {
            dup2(fd[0], STDIN_FILENO); // Redirect stdin from pipe
            close(fd[1]);
            close(fd[0]);

            if (flow.nodes.find(pipe.to) != flow.nodes.end()) {
                executeNode(flow.nodes[pipe.to]);
            } else if (flow.concats.find(pipe.to) != flow.concats.end()) {
                executeConcatenate(flow.concats[pipe.to], flow);
            }
            exit(0);
        }

        close(fd[0]);
        close(fd[1]);
        waitpid(p2, nullptr, 0);
        waitpid(p3, nullptr, 0);
    } else if (p1 < 0) {
        perror("fork failed");
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: ./flow <flow_file> <action>" << endl;
        return 1;
    }

    string flowFile = argv[1];
    string action = argv[2];

    Flow flow;

    if (readFile(flowFile, flow) == 1) {
        cerr << "Error: Could not open file " << flowFile << endl;
        return 1;
    }

    // Execute the specified action
    if (flow.pipes.find(action) != flow.pipes.end()) {
        executePipe(flow, flow.pipes[action]);
    } else if (flow.nodes.find(action) != flow.nodes.end()) {
        executeNode(flow.nodes[action]);
    } else if (flow.concats.find(action) != flow.concats.end()) {
        executeConcatenate(flow.concats[action], flow);
    } else {
        cerr << "Action not found." << endl;
    }

    return 0;
}
