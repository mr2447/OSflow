#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
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
        } else if (line.compare(0, 8, "part_") == 0) {
            size_t idx = stoi(line.substr(5, 1)); // Assumes part index is a single digit
            currCat.part_names[idx] = line.substr(line.find('=') + 1); // Capture part name
            if(idx == currCat.part_names.size()){
                concatenates.push_back(currCat);
            }
        }
    }

    // Close the file
    file.close();
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <file_name>" << endl;
        return 1;
    }
    vector<Node> nodes;
    vector<Pipe> pipes;
    vector<Concatenate> concatenates;

    readFile(argv[1], nodes, pipes, concatenates);

    cout << "Nodes: " << endl;
    for(Node n : nodes){
        cout << n.name << endl;
        cout << n.command << endl;
    }

    cout << "Pipes: " << endl;
    for(Pipe p : pipes){
        cout << p.name << endl;
        cout << p.from << endl;
        cout << p.to << endl;
    }

    cout << "Concatenates: " << endl;
    for(Concatenate c : concatenates)
    {
        cout << c.name << endl;
        cout << c.parts << endl;
        for(string s: c.part_names){
            cout << s << endl;
        }
    }

return 0;}
