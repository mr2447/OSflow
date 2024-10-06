#include <iostream>
#include <string>
#include <vector>
#include <fstream>
using namespace std;

// Function to read the file and print its contents
void readFile(const string& fileName) {
    // Open file
    ifstream file(fileName);

    // Check if the file was opened successfully
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << fileName << endl;
        return;
    }

    // Read the file line by line
    string line;
    while (getline(file, line)) {
        cout << line << endl;  // Output the contents of the file
    }

    // Close the file
    file.close();
}

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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <file_name>" << endl;
        return 1;
    }

    readFile(argv[1]);

return 0;}

