#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cctype>
#include <iomanip> // per setw, setprecision
#include <unordered_set>
#include <map>
#include <algorithm>
#include <cassert>
#include <functional>

using namespace std;


// Fattore: tabella di valori indicizzata dalle variabili nello scope
struct Factor {
    vector<string> scope;
    unordered_map<string, int> domain_sizes;
    vector<double> values;

    int getIndex(const unordered_map<string, int>& assignment) const {
        int index = 0;
        int multiplier = 1;
        for (int i = scope.size() - 1; i >= 0; --i) {
            int val = assignment.at(scope[i]);
            index += val * multiplier;
            multiplier *= domain_sizes.at(scope[i]);
        }
        return index;
    }

    double get(const unordered_map<string, int>& assignment) const {
        return values[getIndex(assignment)];
    }

    void set(const unordered_map<string, int>& assignment, double value) {
        values[getIndex(assignment)] = value;
    }
};

class BIFParser {
public:
    unordered_map<string, int> domain_sizes;
    unordered_map<string, vector<string>> domains;
    unordered_map<string, unordered_map<string,int>> value_indices;
    vector<Factor> factors;

    void parse(const string& filename) {
        ifstream in(filename);
        if (!in) {
            cerr << "Impossibile aprire il file: " << filename << endl;
            return;
        }
        string line;
        while (getline(in, line)) {
            trim(line);
            if (line.empty() || line[0] == '%') continue;
            if (startsWith(line, "variable")) {
                parseVariable(in, line);
            } else if (startsWith(line, "probability")) {
                parseProbability(in, line);
            }
        }
    }

private:
    static void trim(string& s) {
        auto ws_front = s.find_first_not_of(" \t\r\n");
        if (ws_front == string::npos) { s.clear(); return; }
        s.erase(0, ws_front);
        auto ws_back = s.find_last_not_of(" \t\r\n");
        s.erase(ws_back + 1);
    }

    static bool startsWith(const string& s, const string& prefix) {
        return s.rfind(prefix, 0) == 0;
    }

    static string trimCopy(string s) {
        trim(s);
        return s;
    }

    vector<string> split(const string& s, char delim) {
        vector<string> elems;
        stringstream ss(s);
        string item;
        while (getline(ss, item, delim)) {
            trim(item);
            if (!item.empty()) elems.push_back(item);
        }
        return elems;
    }

    void parseVariable(ifstream& in, string& headerLine) {
        stringstream ss(headerLine);
        string tmp, varName;
        ss >> tmp >> varName;
        string line;
        int size = 0;
        vector<string> vals;
        while (getline(in, line)) {
            trim(line);
            if (startsWith(line, "type discrete")) {
                auto p1 = line.find('[');
                auto p2 = line.find(']');
                size = stoi(line.substr(p1+1, p2-p1-1));
                auto b1 = line.find('{');
                auto b2 = line.find('}');
                string inside = line.substr(b1+1, b2-b1-1);
                vals = split(inside, ',');
            } else if (line.find('}') != string::npos) {
                break;
            }
        }
        domain_sizes[varName] = size;
        domains[varName] = vals;
        value_indices[varName] = {};
        for (int i = 0; i < (int)vals.size(); ++i) {
            value_indices[varName][vals[i]] = i;
        }
    }

    void parseProbability(ifstream& in, string& headerLine) {
        auto p1 = headerLine.find('(');
        auto p2 = headerLine.find(')');
        string inside = headerLine.substr(p1+1, p2-p1-1);
        string child;
        vector<string> parents;
        auto bar = inside.find('|');
        if (bar != string::npos) {
            child = trimCopy(inside.substr(0, bar));
            string parStr = inside.substr(bar+1);
            parents = split(parStr, ',');
        } else {
            child = trimCopy(inside);
        }
        for (auto& p : parents) trim(p);
        vector<string> scope = parents;
        scope.push_back(child);

        Factor f;
        f.scope = scope;
        for (auto& var : scope) f.domain_sizes[var] = domain_sizes[var];
        int totalSize = 1;
        for (auto& var : scope) totalSize *= domain_sizes[var];
        f.values.assign(totalSize, 0.0);

        string line;
        while (getline(in, line)) {
            trim(line);
            if (line.empty()) continue;
            if (line.find('}') != string::npos) break;
            if (startsWith(line, "table")) {
                auto pos = line.find("table") + 5;
                auto semi = line.find(';', pos);
                string nums = line.substr(pos, semi-pos);
                auto vals = split(nums, ',');
                for (int i = 0; i < (int)vals.size(); ++i) {
                    unordered_map<string,int> assignment;
                    assignment[child] = i;
                    f.set(assignment, stod(vals[i]));
                }
            } else if (line[0] == '(') {
                auto rp = line.find(')');
                string parAss = line.substr(1, rp-1);
                auto parVals = split(parAss, ',');
                unordered_map<string,int> assignment;
                for (int i = 0; i < (int)parents.size(); ++i) {
                    assignment[parents[i]] = value_indices[parents[i]][parVals[i]];
                }
                auto rest = line.substr(rp+1);
                auto semi = rest.find(';');
                rest = rest.substr(0, semi);
                auto childVals = split(rest, ',');
                for (int i = 0; i < (int)childVals.size(); ++i) {
                    assignment[child] = i;
                    f.set(assignment, stod(childVals[i]));
                }
            }
        }
        factors.push_back(f);
    }
};





Factor restrictFactor(const Factor& f, const unordered_map<string, int>& evidence) {
    Factor result;
    result.scope = {};
    result.domain_sizes = {};
    for (const string& var : f.scope) {
        if (!evidence.count(var)) {
            result.scope.push_back(var);
            result.domain_sizes[var] = f.domain_sizes.at(var);
        }
    }

    int totalSize = 1;
    for (const auto& [var, size] : result.domain_sizes)
        totalSize *= size;
    result.values.resize(totalSize, 0.0);

    unordered_map<string, int> fullAssignment;
    function<void(int)> recurse = [&](int idx) {
        if (idx == f.scope.size()) {
            bool matches = true;
            for (const auto& [var, val] : evidence) {
                if (!f.domain_sizes.count(var)) continue; // evidenza non nello scope del fattore
                if (fullAssignment[var] != val) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                unordered_map<string, int> subAssignment;
                for (const auto& var : result.scope)
                    subAssignment[var] = fullAssignment[var];
                result.set(subAssignment, f.get(fullAssignment));
            }
            return;
        }
        string var = f.scope[idx];
        int dom = f.domain_sizes.at(var);
        for (int v = 0; v < dom; ++v) {
            fullAssignment[var] = v;
            recurse(idx + 1);
        }
    };
    recurse(0);
    return result;
}

Factor multiplyFactors(const Factor& f1, const Factor& f2) {
    Factor result;
    unordered_set<string> varSet(f1.scope.begin(), f1.scope.end());
    for (const string& v : f2.scope) varSet.insert(v);
    result.scope.assign(varSet.begin(), varSet.end());

    for (const string& v : result.scope)
        result.domain_sizes[v] = (f1.domain_sizes.count(v) ? f1.domain_sizes.at(v) : f2.domain_sizes.at(v));

    int totalSize = 1;
    for (const auto& [var, size] : result.domain_sizes)
        totalSize *= size;
    result.values.resize(totalSize);

    unordered_map<string, int> assignment;
    function<void(int)> recurse = [&](int idx) {
        if (idx == result.scope.size()) {
            unordered_map<string, int> a1, a2;
            for (const auto& v : f1.scope) a1[v] = assignment[v];
            for (const auto& v : f2.scope) a2[v] = assignment[v];
            result.set(assignment, f1.get(a1) * f2.get(a2));
            return;
        }
        string var = result.scope[idx];
        for (int v = 0; v < result.domain_sizes[var]; ++v) {
            assignment[var] = v;
            recurse(idx + 1);
        }
    };
    recurse(0);
    return result;
}

Factor sumOut(const Factor& f, const string& var) {
    Factor result;
    for (const string& v : f.scope)
        if (v != var) result.scope.push_back(v);

    for (const auto& [v, size] : f.domain_sizes)
        if (v != var) result.domain_sizes[v] = size;

    int totalSize = 1;
    for (const auto& [_, size] : result.domain_sizes)
        totalSize *= size;
    result.values.resize(totalSize, 0.0);

    unordered_map<string, int> assignment;
    function<void(int)> recurse = [&](int idx) {
        if (idx == f.scope.size()) {
            unordered_map<string, int> reduced;
            for (const string& v : result.scope)
                reduced[v] = assignment[v];
            result.values[result.getIndex(reduced)] += f.get(assignment);
            return;
        }
        string varName = f.scope[idx];
        for (int i = 0; i < f.domain_sizes.at(varName); ++i) {
            assignment[varName] = i;
            recurse(idx + 1);
        }
    };
    recurse(0);
    return result;
}

Factor normalize(const Factor& f) {
    Factor result = f;
    double total = 0.0;
    for (double v : result.values) total += v;
    for (double& v : result.values) v /= total;
    return result;
}




Factor variableElimination(vector<Factor> factors, const string& query, const vector<string>& elimOrder, const unordered_map<string, int>& evidence) {
    for (auto& f : factors)
        f = restrictFactor(f, evidence);

    for (const string& var : elimOrder) {
        vector<Factor> related, others;
        for (const auto& f : factors) {
            if (find(f.scope.begin(), f.scope.end(), var) != f.scope.end())
                related.push_back(f);
            else
                others.push_back(f);
        }
        if (!related.empty()) {
            Factor product = related[0];
            for (size_t i = 1; i < related.size(); ++i)
                product = multiplyFactors(product, related[i]);
            Factor reduced = sumOut(product, var);
            others.push_back(reduced);
        }
        factors = others;
    }

    Factor result = factors[0];
    for (size_t i = 1; i < factors.size(); ++i)
        result = multiplyFactors(result, factors[i]);

    return normalize(result);
}



// Uso: ./variable_elimination [file.bif] [query] [var=valore ...]
int main(int argc, char* argv[]) {
    string filename = argc > 1 ? argv[1] : "asia.bif";
    string search_node = argc > 2 ? argv[2] : "lung";
    BIFParser parser;
    parser.parse(filename);
    if (parser.factors.empty()) return 1;

    // Stampa lo scope dei fattori caricati
    for (const auto& f : parser.factors) {
        cout << "Factor scope: ";
        for (const auto& v : f.scope) cout << v << " ";
        cout << endl;
    }

    if (!parser.domain_sizes.count(search_node)) {
        cerr << "Variabile sconosciuta: " << search_node << endl;
        return 1;
    }

    // Evidenza nel formato var=valore (es. smoke=yes)
    unordered_map<string, int> evidence;
    for (int i = 3; i < argc; ++i) {
        string arg = argv[i];
        auto eq = arg.find('=');
        if (eq == string::npos) {
            cerr << "Evidenza non valida: " << arg << " (usa var=valore)" << endl;
            return 1;
        }
        string var = arg.substr(0, eq), val = arg.substr(eq + 1);
        if (!parser.value_indices.count(var) || !parser.value_indices[var].count(val)) {
            cerr << "Evidenza sconosciuta: " << arg << endl;
            return 1;
        }
        evidence[var] = parser.value_indices[var][val];
    }

    // Ordine di eliminazione: tutte le variabili tranne query ed evidenza
    vector<string> elimOrder;
    for (const auto& [var, _] : parser.domain_sizes)
        if (var != search_node && !evidence.count(var))
            elimOrder.push_back(var);
    sort(elimOrder.begin(), elimOrder.end());

    Factor result = variableElimination(parser.factors, search_node, elimOrder, evidence);

    cout << "\nP(" << search_node;
    if (!evidence.empty()) {
        cout << " |";
        for (int i = 3; i < argc; ++i) cout << " " << argv[i];
    }
    cout << "):" << endl;
    for (int i = 0; i < parser.domain_sizes[search_node]; ++i) {
        unordered_map<string, int> a = {{search_node, i}};
        cout << "  " << setw(6) << left << parser.domains[search_node][i]
             << fixed << setprecision(4) << result.get(a) << endl;
    }

    return 0;
}
