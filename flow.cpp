#include <iostream>
#include <string>
#include <vector>
using namespace std;

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
