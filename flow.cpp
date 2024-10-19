#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <cstring>
#include <fcntl.h>


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
void printOutput(int fd);  // Function to print captured output

// Helper function to capture and print output from a pipe
void printCapturedOutput(int fd) {
    char buffer[1024];
    ssize_t n;
    while ((n = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';  // Null-terminate the buffer
        printf("%s", buffer);  // Print the buffer to stdout
    }
}

void executeConcatenate(const Concatenate& concat, Flow& flow) {
    int fd[2];  // Pipe for capturing combined output between parts
    if (pipe(fd) == -1) {
        perror("pipe failed");
        exit(1);
    }

    // First child process for part_0 (e.g., cat foo.txt)
    pid_t p2 = fork();
    if (p2 == -1) {
        perror("fork failed for part_0");
        exit(1);
    }

    if (p2 == 0) {
        // Child process 1 (part_0)
        dup2(fd[1], STDOUT_FILENO);  // Redirect stdout to pipe's write end
        close(fd[0]);  // Close unused read end of the pipe
        close(fd[1]);  // Close write end after dup2

        const string& partName = concat.part_names[0];  // First part
        // printf("Executing part_0: %s\n", partName.c_str());
        fflush(stdout);  // Ensure the output is flushed immediately

        if (flow.nodes.find(partName) != flow.nodes.end()) {
            executeNode(flow.nodes[partName]);  // Execute node (e.g., cat foo.txt)
        } else if (flow.pipes.find(partName) != flow.pipes.end()) {
            executePipe(flow, flow.pipes[partName]);  // Execute pipe if it's a pipe
        } else if (flow.concats.find(partName) != flow.concats.end()) {
            executeConcatenate(flow.concats[partName], flow);  // Nested concatenate
        }

        // Ensure a newline is printed after the first part's output
        printf("\n");
        exit(0);  // Exit child process after execution
    }

    // Parent waits for the first child (part_0) to complete before starting part_1
    waitpid(p2, nullptr, 0);  // Ensure part_0 finishes

    // Second child process for part_1 (e.g., sed 's/o/u/g')
    pid_t p3 = fork();
    if (p3 == -1) {
        perror("fork failed for part_1");
        exit(1);
    }

    if (p3 == 0) {
        // Child process 2 (part_1)
        dup2(fd[1], STDOUT_FILENO);  // Redirect stdout to pipe's write end
        close(fd[0]);  // Close unused read end of the pipe
        close(fd[1]);  // Close write end after dup2

        const string& partName = concat.part_names[1];  // Second part
        // printf("Executing part_1: %s\n", partName.c_str());
        fflush(stdout);  // Ensure the output is flushed immediately

        if (flow.nodes.find(partName) != flow.nodes.end()) {
            executeNode(flow.nodes[partName]);  // Execute node (e.g., sed 's/o/u/g')
        } else if (flow.pipes.find(partName) != flow.pipes.end()) {
            executePipe(flow, flow.pipes[partName]);  // Execute pipe if it's a pipe
        } else if (flow.concats.find(partName) != flow.concats.end()) {
            executeConcatenate(flow.concats[partName], flow);  // Nested concatenate
        }

        // Ensure a newline is printed after the second part's output
        printf("\n");
        exit(0);  // Exit child process after execution
    }

    // Parent process closes unused pipe ends
    close(fd[1]);  // Close write end in the parent (used by children)

    // Wait for both child processes to complete
    waitpid(p3, nullptr, 0);  // Wait for part_1 to finish

    // Now read the combined output from the first and second parts
    // printf("\nReading combined output of all parts:\n");
    printCapturedOutput(fd[0]);  // This will capture both the outputs from the pipe
    close(fd[0]);  // Close the read end of the pipe after reading

    // Print the combined output being passed to wc (simulated, for debugging)
    // printf("\nCombined output passed to wc:\n");
    fflush(stdout);
    printCapturedOutput(fd[0]);  // Ensure we're reading from the correct fd, not stdin

    close(fd[0]);  // Close the read end of the pipe after reading
}
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

// Helper function to print captured output from a file descriptor
void printOutput(int fd) {
    char buffer[1024];
    ssize_t n;
    while ((n = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';  // Null-terminate the buffer
        printf("%s", buffer);  // Print the buffer
    }
}

// Function to execute a node
void executeNode(const Node& node) {
    pid_t pid = fork();

    if (pid == 0) { // Child process
        char* args[100];
        string command = node.command;
        istringstream iss(command);
        string arg;
        int i = 0;

        while (iss >> arg) {
            // If the argument is wrapped in single quotes, remove the quotes
            if (arg.front() == '\'' && arg.back() == '\'') {
                arg = arg.substr(1, arg.size() - 2);  // Strip the quotes
            }

            // Store the argument in args[] array
            args[i] = new char[arg.size() + 1];
            strcpy(args[i], arg.c_str());
            i++;
        }
        args[i] = nullptr;  // Null-terminate the argument list

        // Execute the command
        if (execvp(args[0], args) < 0) {
            perror("execvp failed");
            exit(1);
        }

        // Flush stdout to ensure all output is printed before exit
        fflush(stdout);
    } else if (pid < 0) {
        perror("fork failed");
    } else {
        waitpid(pid, nullptr, 0); // Parent process waits for child
    }
}

// Function to execute a pipe between two nodes or concatenates
void executePipe(Flow& flow, const Pipe& pipeStruct) {
    int fd1[2], fd2[2];
    if (pipe(fd1) == -1 || pipe(fd2) == -1) {
        perror("pipeStruct failed");
        exit(1);
    }

    // First child process (p2) for 'from' part of the pipe
    pid_t p2 = fork();
    if (p2 == -1) {
        perror("fork failed for p2");
        exit(1);
    }

    if (p2 == 0) {
        // Child process for 'from' side of the pipe (p2)
        dup2(fd1[1], STDOUT_FILENO);  // Redirect stdout to the pipe's write end
        close(fd1[0]);  // Close unused read end
        close(fd1[1]);  // Close write end after dup2

        const string& from = pipeStruct.from;

        // Check if 'from' is a node, pipe, or concatenate
        if (flow.nodes.find(from) != flow.nodes.end()) {
            executeNode(flow.nodes[from]);  // Execute node (e.g., cat foo.txt)
        } else if (flow.pipes.find(from) != flow.pipes.end()) {
            executePipe(flow, flow.pipes[from]);  // Recursive call to executePipe()
        } else if (flow.concats.find(from) != flow.concats.end()) {
            executeConcatenate(flow.concats[from], flow);  // Execute concatenate
        }

        exit(0);  // Exit child process after execution
    }

    // Parent waits for the first child to finish
    waitpid(p2, nullptr, 0);

    // Second child process (p3) for 'to' part of the pipe
    pid_t p3 = fork();
    if (p3 == -1) {
        perror("fork failed for p3");
        exit(1);
    }

    if (p3 == 0) {
        // Child process for 'to' side of the pipe (p3)
        dup2(fd1[0], STDIN_FILENO);  // Redirect stdin to the pipe's read end (from p2)
        dup2(fd2[1], STDOUT_FILENO); // Redirect stdout to the second pipe's write end
        close(fd1[1]);  // Close unused write end of fd1
        close(fd1[0]);  // Close read end of fd1 after dup2
        close(fd2[0]);  // Close unused read end of fd2
        close(fd2[1]);  // Close write end of fd2 after dup2

        const string& to = pipeStruct.to;

        // Check if 'to' is a node, pipe, or concatenate
        if (flow.nodes.find(to) != flow.nodes.end()) {
            executeNode(flow.nodes[to]);  // Execute node (e.g., sed 's/o/u/g')
        } else if (flow.pipes.find(to) != flow.pipes.end()) {
            executePipe(flow, flow.pipes[to]);  // Recursive call to executePipe()
        } else if (flow.concats.find(to) != flow.concats.end()) {
            executeConcatenate(flow.concats[to], flow);  // Execute concatenate
        }

        exit(0);  // Exit child process after execution
    }

    // Parent process closes unused ends
    close(fd1[0]);
    close(fd1[1]);
    close(fd2[1]);  // Close write end of fd2 (used by p3)

    // Wait for both children to finish
    waitpid(p2, nullptr, 0);
    waitpid(p3, nullptr, 0);

    // Print the output of p3
    // printf("Output of p3 (sed_o_u or similar):\n");
    printOutput(fd2[0]);

    close(fd2[0]);  // Close the read end of the second pipe after reading
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
