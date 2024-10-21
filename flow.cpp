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
    int fd[2];  // Pipe for capturing the combined output of all parts
    if (pipe(fd) == -1) {
        perror("pipe failed");
        exit(1);
    }

    for (int i = 0; i < concat.parts; i++) {
        // Fork a new child process for each part
        pid_t p = fork();
        if (p == -1) {
            perror("fork failed");
            exit(1);
        }

        if (p == 0) {
            // Child process (for each part)
            // Redirect stdout to the pipe's write end for all parts
            dup2(fd[1], STDOUT_FILENO);  // Redirect stdout to the pipe
            close(fd[0]);  // Close unused read end of the pipe
            close(fd[1]);  // Close write end after dup2

            const string& partName = concat.part_names[i];  // Get the current part's name

            if (flow.nodes.find(partName) != flow.nodes.end()) {
                executeNode(flow.nodes[partName]);  // Execute node (e.g., cat foo.txt)
            } else if (flow.pipes.find(partName) != flow.pipes.end()) {
                executePipe(flow, flow.pipes[partName]);  // Execute pipe if it's a pipe
            } else if (flow.concats.find(partName) != flow.concats.end()) {
                executeConcatenate(flow.concats[partName], flow);  // Nested concatenate
            }

            exit(0);  // Exit child process after execution
        }

        // Parent process waits for the current child to complete
        waitpid(p, nullptr, 0);
    }

    // Parent process: after all parts are done, output the combined results of all parts to stdout
    close(fd[1]);  // Close the write end in the parent (only reading now)

    // Redirect the concatenated output to stdout (which can then be piped into another command)
    char buffer[1024];
    ssize_t bytesRead;

    // Read the combined output from the pipe and write it to stdout using write()
    while ((bytesRead = read(fd[0], buffer, sizeof(buffer))) > 0) {
        write(STDOUT_FILENO, buffer, bytesRead);  // Output the buffer content to stdout
    }

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
// Function to execute a pipe between two nodes or concatenates
void executePipe(Flow& flow, const Pipe& pipeStruct) {
    int fd1[2], fd2[2];
    if (pipe(fd1) == -1 || pipe(fd2) == -1) {
        perror("pipeStruct failed");
        exit(1);
    }


    // First child process (p2) for 'from' part of the pipe
    pid_t p2 = fork();
    // printf("Fork returned: %d\n", p2);
    // fflush(stdout);  // Force the output to flush

    if (p2 == -1) {
        perror("fork failed for p2");
        exit(1);
    }

    if (p2 == 0) {
        // printf("In child process p2\n");
        // fflush(stdout);  // Force output in child to flush

        // Child process for 'from' side of the pipe (p2)
        dup2(fd1[1], STDOUT_FILENO);  // Redirect stdout to the pipe's write end
        close(fd1[0]);  // Close unused read end
        close(fd1[1]);  // Close write end after dup2

        const string& from = pipeStruct.from;

        // Check if 'from' is a node, pipe, or concatenate
        if (flow.nodes.find(from) != flow.nodes.end()) {
            // fprintf(stderr, "Debug: In child process p2: %s \n", flow.nodes[from].name.c_str());
            // fflush(stderr);  // Flush stderr, since it's not redirected
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
    // printf("Fork returned: %d\n", p3);
    // fflush(stdout);  // Force the output to flush

    if (p3 == -1) {
        perror("fork failed for p3");
        exit(1);
    }

    if (p3 == 0) {
        // printf("In child process p3\n");
        // fflush(stdout);  // Force output in child to flush

        // Child process for 'to' side of the pipe (p3)
        dup2(fd1[0], STDIN_FILENO);  // Redirect stdin to the pipe's read end (from p2)
        close(fd1[1]);  // Close unused write end of fd1
        close(fd1[0]);  // Close read end of fd1 after dup2

        // No need for dup2(fd2[1], STDOUT_FILENO) -- p3 outputs directly to the terminal

        const string& to = pipeStruct.to;

        // Check if 'to' is a node, pipe, or concatenate
        if (flow.nodes.find(to) != flow.nodes.end()) {
            // printf("We found pipe action: %s\n", flow.nodes[to].command.c_str());
            // fflush(stdout);
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
