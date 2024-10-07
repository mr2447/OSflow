#include <iostream>
#include <string>
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
    string name;      // Unique name for the concatenate operation
    int parts;             // Number of parts
    vector<string> part_names; // Names of the parts (nodes/pipes)
};

struct Flow {
    vector<Node> nodes;         // Collection of nodes
    vector<Pipe> pipes;         // Collection of pipes
    vector<Concatenate> concats; // Collection of concatenation operations
};

// Function to read the file and print its contents
void readFile(const string& fileName, vector<Node>& nodes, vector<Pipe>& pipes, vector<Concatenate>& concatenates) {
    // Open file
    ifstream file(fileName);

    // Check if the file was opened successfully
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << fileName << endl;
        return;
    }

    bool nodeFlag = 0, pipeFlag = 0, concatFlag = 0;
    
    Node currNode;
    Pipe currPipe;
    Concatenate currCat;

    // Read the file line by line
    string line;
    while (getline(file, line)) {
        if (line.compare(0, 5, "node=") == 0) {
            currNode.name = line.substr(5); // Capture node name
        } else if (line.compare(0, 8, "command=") == 0) {
            currNode.command = line.substr(8); // Capture command
            nodes.push_back(currNode);
        } else if (line.compare(0, 5, "pipe=") == 0) {
            currPipe.name = line.substr(5); // Capture pipe name
        } else if (line.compare(0, 5, "from=") == 0) {
            currPipe.from = line.substr(5); // Capture from node
        } else if (line.compare(0, 3, "to=") == 0) {
            currPipe.to = line.substr(3); // Capture to node
            pipes.push_back(currPipe);
        } else if (line.compare(0, 12, "concatenate=") == 0) {
            currCat.name = line.substr(12); // Capture concatenate name
        } else if (line.compare(0, 6, "parts=") == 0) {
            currCat.parts = stoi(line.substr(6)); // Capture number of parts
            currCat.part_names.resize(currCat.parts); // Resize vector for part names
            cout << currCat.part_names.size() << endl;
        } else if (line.compare(0, 5, "part_") == 0) {
            int idx = stoi(line.substr(5, 1)); // Assumes part index is a single digit
            currCat.part_names[idx] = line.substr(line.find('=') + 1); // Capture part name
            if(idx+1 == currCat.part_names.size()){
                concatenates.push_back(currCat);
            }
        }
    }

    // Close the file
    file.close();
}

// Function to execute a command in a node
void executeNode(const Node& node) {
    // Create a new process
    pid_t pid = fork();

    if (pid == 0) { // Child process
        // Execute the command
        if (execlp(node.command.c_str(), node.command.c_str(), nullptr) < 0) {
            perror("execlp failed"); // Print error if exec fails
            exit(1); // Exit child process on failure
        }
    } else if (pid < 0) {
        perror("fork failed"); // Error handling if fork fails
    } else {
        waitpid(pid, nullptr, 0); // Parent process waits for child to finish
    }
}
// Function to execute a pipe between two nodes
void executePipe(const Pipe& Pipe, const std::vector<Node>& nodes) {
    pid_t p1 = fork();
    if (p1 == 0) { // First child process
        // Create a pipe
        int fd[2];
        pipe(fd);

        // Fork the first command
        pid_t p2 = fork();
        if (p2 == 0) { // Execute the 'from' node
            dup2(fd[1], STDOUT_FILENO); // Redirect stdout to pipe
            close(fd[0]); // Close unused read end
            close(fd[1]); // Close write end after dup
            // Find and execute the command
            for (const auto& node : nodes) {
                if (node.name == Pipe.from) {
                    executeNode(node);
                    break;
                }
            }
            exit(0);
        }

        pid_t p3 = fork();
        if (p3 == 0) { // Execute the 'to' node
            dup2(fd[0], STDIN_FILENO); // Redirect stdin from pipe
            close(fd[1]); // Close unused write end
            close(fd[0]); // Close read end after dup
            // Find and execute the command
            for (const auto& node : nodes) {
                if (node.name == Pipe.to) {
                    executeNode(node);
                    break;
                }
            }
            exit(0);
        }

        // Close pipe in parent
        close(fd[0]);
        close(fd[1]);
        waitpid(p2, nullptr, 0); // Wait for first command to finish
        waitpid(p3, nullptr, 0); // Wait for second command to finish
    } else if (p1 < 0) {
        perror("fork failed");
    }
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./flow <flow_file> <action>" << std::endl;
        return 1;
    }

    string flowFile = argv[1];
    string action = argv[2];

    vector<Node> nodes;
    vector<Pipe> pipes;
    vector<Concatenate> concatenates;

    readFile(argv[1], nodes, pipes, concatenates);

    // Execute the specified action
    for (const auto& pipe : pipes) {
        if (pipe.name == action) {
            executePipe(pipe, nodes);
            break;
        }
    }

    // cout << "Nodes: " << endl;
    // for(Node n : nodes){
    //     cout << "node name: " << n.name << endl;
    //     cout << "node command: " << n.command << endl;
    // }

    // cout << "Pipes: " << endl;
    // for(Pipe p : pipes){
    //     cout << "pipe name: " << p.name << endl;
    //     cout << "pipe from: " << p.from << endl;
    //     cout << "pipe to: " << p.to << endl;
    // }

    // cout << "Concatenates: " << endl;
    // for(Concatenate c : concatenates)
    // {
    //     cout << "cat name: " << c.name << endl;
    //     cout << "cat parts: " << c.parts << endl;
    //     int i = 0;
    //     for(string s: c.part_names){
    //         cout << "cat part_" << i++ << ": " << s << endl;
    //     }
    // }

return 0;}
