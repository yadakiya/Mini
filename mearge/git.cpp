#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <ctime>
#include <sstream>

using namespace std;
namespace fs = filesystem;

const string MINIGIT_DIR = ".minigit";
const string OBJECTS_DIR = MINIGIT_DIR + "/objects";
const string COMMITS_DIR = MINIGIT_DIR + "/commits";
const string HEAD_FILE = MINIGIT_DIR + "/HEAD";
const string BRANCHES_FILE = MINIGIT_DIR + "/branches";
const string INDEX_FILE = MINIGIT_DIR + "/index";

string generateHash(const string& content) {
    hash<string> hasher;
    return to_string(hasher(content));
}

string currentTime() {
    time_t now = time(nullptr);
    return string(ctime(&now));
}

void init() {
    if (fs::exists(MINIGIT_DIR)) {
        cout << "MiniGit repository already exists.\n";
        return;
    }
    fs::create_directory(MINIGIT_DIR);
    fs::create_directory(OBJECTS_DIR);
    fs::create_directory(COMMITS_DIR);
    ofstream(HEAD_FILE) << "main";
    ofstream(BRANCHES_FILE) << "main:null\n";
    ofstream(INDEX_FILE);
    cout << "Initialized empty MiniGit repository.\n";
}

void add(const string& filename) {
    ifstream inFile(filename);
    if (!inFile) {
        cout << "File not found: " << filename << endl;
        return;
    }
    stringstream buffer;
    buffer << inFile.rdbuf();
    string content = buffer.str();
    string hash = generateHash(content);

    ofstream objectOut(OBJECTS_DIR + "/" + hash);
    objectOut << content;

    ofstream indexOut(INDEX_FILE, ios::app);
    indexOut << filename << ":" << hash << endl;

    cout << "Staged: " << filename << endl;
}

string getHeadBranch() {
    ifstream headIn(HEAD_FILE);
    string branch;
    getline(headIn, branch);
    return branch;
}

string getBranchCommit(const string& branch) {
    ifstream in(BRANCHES_FILE);
    string line;
    while (getline(in, line)) {
        size_t pos = line.find(":");
        if (line.substr(0, pos) == branch)
            return line.substr(pos + 1);
    }
    return "null";
}

void updateBranch(const string& branch, const string& commitHash) {
    ifstream in(BRANCHES_FILE);
    stringstream out;
    string line;
    while (getline(in, line)) {
        size_t pos = line.find(":");
        if (line.substr(0, pos) == branch)
            out << branch << ":" << commitHash << "\n";
        else
            out << line << "\n";
    }
    ofstream outFile(BRANCHES_FILE);
    outFile << out.str();
}

void commit(const string& message) {
    ifstream indexIn(INDEX_FILE);
    stringstream files;
    string line;
    while (getline(indexIn, line)) {
        files << line << endl;
    }

    string timestamp = currentTime();
    string branch = getHeadBranch();
    string parent = getBranchCommit(branch);

    string metadata = "msg:" + message + "\n" +
                      "time:" + timestamp +
                      "parent:" + parent + "\n" +
                      "branch:" + branch + "\n";

    string commitHash = generateHash(metadata + files.str());
    ofstream commitFile(COMMITS_DIR + "/" + commitHash);
    commitFile << metadata << files.str();

    updateBranch(branch, commitHash);
    ofstream(INDEX_FILE);
    cout << "Committed. Hash: " << commitHash << endl;
}

void log() {
    string current = getBranchCommit(getHeadBranch());
    while (current != "null") {
        ifstream in(COMMITS_DIR + "/" + current);
        string line;
        cout << "Commit: " << current << endl;
        while (getline(in, line)) {
            if (line.rfind("msg:", 0) == 0 || line.rfind("time:", 0) == 0 || line.rfind("parent:", 0) == 0)
                cout << line << endl;
        }
        cout << "-----------------------\n";

        in.clear();
        in.seekg(0);
        while (getline(in, line)) {
            if (line.rfind("parent:", 0) == 0) {
                current = line.substr(7);
                break;
            }
        }
    }
}

void branch(const string& name) {
    string current = getBranchCommit(getHeadBranch());
    ofstream out(BRANCHES_FILE, ios::app);
    out << name << ":" << current << endl;
    cout << "Branch created: " << name << endl;
}

void merge(const string& target) {
    string base = getBranchCommit(getHeadBranch());
    string other = getBranchCommit(target);

    ifstream a(COMMITS_DIR + "/" + base);
    ifstream b(COMMITS_DIR + "/" + other);

    unordered_map<string, string> files;
    string line;

    while (getline(b, line)) {
        if (line.find(":") != string::npos && line.find("msg:") != 0 && line.find("time:") != 0 && line.find("parent:") != 0)
            files[line.substr(0, line.find(":"))] = line.substr(line.find(":") + 1);
    }

    while (getline(a, line)) {
        if (line.find(":") != string::npos && line.find("msg:") != 0 && line.find("time:") != 0 && line.find("parent:") != 0) {
            string file = line.substr(0, line.find(":"));
            string hash = line.substr(line.find(":") + 1);
            if (files.count(file) && files[file] != hash)
                cout << "CONFLICT: both modified " << file << endl;
            files[file] = hash;
        }
    }

    ofstream indexOut(INDEX_FILE);
    for (auto& pair : files)
        indexOut << pair.first << ":" << pair.second << endl;

    commit("Merged branch " + target);
}

void checkout(const string& target) {
    string commitHash = target;
    ifstream branchesIn(BRANCHES_FILE);
    string line;
    bool isBranch = false;
    while (getline(branchesIn, line)) {
        size_t pos = line.find(":");
        if (line.substr(0, pos) == target) {
            commitHash = line.substr(pos + 1);
            isBranch = true;
            break;
        }
    }

    string commitPath = COMMITS_DIR + "/" + commitHash;
    if (!fs::exists(commitPath)) {
        cout << "Invalid branch or commit.\n";
        return;
    }

    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.path().filename() == ".minigit") continue;
        if (fs::is_regular_file(entry)) fs::remove(entry);
    }

    ifstream commitIn(commitPath);
    while (getline(commitIn, line)) {
        if (line.rfind("msg:", 0) == 0 || line.rfind("time:", 0) == 0 || 
            line.rfind("parent:", 0) == 0 || line.rfind("branch:", 0) == 0)
            continue;

        size_t pos = line.find(":");
        string filename = line.substr(0, pos);
        string hash = line.substr(pos + 1);
        ifstream objIn(OBJECTS_DIR + "/" + hash);
        ofstream outFile(filename);
        outFile << objIn.rdbuf();
    }

    ofstream headOut(HEAD_FILE);
    headOut << (isBranch ? target : "detached:" + commitHash);
    cout << "Checked out " << target << endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Commands: init, add <file>, commit -m <msg>, log, branch <name>, merge <branch>, checkout <branch|commit>\n";
        return 1;
    }

    string cmd = argv[1];
    if (cmd == "init") init();
    else if (cmd == "add" && argc >= 3) add(argv[2]);
    else if (cmd == "commit" && argc >= 4 && string(argv[2]) == "-m") commit(argv[3]);
    else if (cmd == "log") log();
    else if (cmd == "branch" && argc >= 3) branch(argv[2]);
    else if (cmd == "merge" && argc >= 3) merge(argv[2]);
    else if (cmd == "checkout" && argc >= 3) checkout(argv[2]);
    else cout << "Invalid command or arguments.\n";
    return 0;
}