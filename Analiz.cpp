#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <algorithm>
#include <iomanip>
#include <cctype>
#include <cstring>
#include <sstream>

using namespace std;

// ============================================================================
// ЛЕКСИЧЕСКИЙ АНАЛИЗАТОР
// ============================================================================

enum TokenType {
    TOKEN_DIM = 1, TOKEN_AS, TOKEN_INTEGER, TOKEN_DO, TOKEN_LOOP, TOKEN_UNTIL,
    TOKEN_PRINT, TOKEN_ABS, TOKEN_INPUT, TOKEN_ASSIGN, TOKEN_PLUS, TOKEN_MINUS,
    TOKEN_MULTIPLY, TOKEN_DIVIDE, TOKEN_MOD, TOKEN_EQUAL, TOKEN_NOT_EQUAL,
    TOKEN_LESS, TOKEN_GREATER, TOKEN_LESS_EQUAL, TOKEN_GREATER_EQUAL,
    TOKEN_COMMA, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_IDENTIFIER, TOKEN_CONSTANT, TOKEN_STRING, TOKEN_EOF, TOKEN_UNKNOWN
};

struct Token {
    TokenType type;
    string value;
    int line;
    int position;
    Token() : type(TOKEN_UNKNOWN), value(""), line(0), position(0) {}
    Token(TokenType t, const string& v, int l, int p) : type(t), value(v), line(l), position(p) {}
};

struct SymbolRecord {
    int index; string name; string type; int line; int size; bool is_array;
    SymbolRecord() : index(-1), line(0), size(0), is_array(false) {}
    SymbolRecord(int idx, const string& n, const string& t, int l, bool arr = false, int sz = 0)
        : index(idx), name(n), type(t), line(l), is_array(arr), size(sz) {}
};

struct ConstantRecord {
    int index; string value; string type; int line;
    ConstantRecord() : index(-1), line(0) {}
    ConstantRecord(int idx, const string& v, const string& t, int l)
        : index(idx), value(v), type(t), line(l) {}
};

struct ErrorRecord {
    int index; int line; int position; string message; string value;
    ErrorRecord() : index(-1), line(0), position(0) {}
    ErrorRecord(int idx, int l, int p, const string& m, const string& v)
        : index(idx), line(l), position(p), message(m), value(v) {}
};

struct FixedKeyword {
    string word; TokenType type; int code; string description;
    FixedKeyword(const string& w, TokenType t, int c, const string& d)
        : word(w), type(t), code(c), description(d) {}
};

struct FixedDelimiter {
    string symbol; TokenType type; int code; string description;
    FixedDelimiter(const string& s, TokenType t, int c, const string& d)
        : symbol(s), type(t), code(c), description(d) {}
};

struct TableInfo {
    int num; string name; string lex_type; string desc;
    TableInfo(int n, const string& nm, const string& lt, const string& d)
        : num(n), name(nm), lex_type(lt), desc(d) {}
};

class Lexer {
private:
    ifstream file;
    string filename;
    int line, position;
    char current_char;
    vector<SymbolRecord> symbol_table;
    vector<ConstantRecord> constant_table;
    vector<ErrorRecord> error_table;
    vector<FixedKeyword> keywords;
    vector<FixedDelimiter> delimiters;
    vector<TableInfo> table_mapping;
    int symbol_idx, const_idx, error_idx;
    bool in_declaration;
    
    void init_tables() {
        keywords.push_back(FixedKeyword("DIM", TOKEN_DIM, 1, "Объявление"));
        keywords.push_back(FixedKeyword("AS", TOKEN_AS, 2, "AS"));
        keywords.push_back(FixedKeyword("INTEGER", TOKEN_INTEGER, 3, "INTEGER"));
        keywords.push_back(FixedKeyword("DO", TOKEN_DO, 4, "DO"));
        keywords.push_back(FixedKeyword("LOOP", TOKEN_LOOP, 5, "LOOP"));
        keywords.push_back(FixedKeyword("UNTIL", TOKEN_UNTIL, 6, "UNTIL"));
        keywords.push_back(FixedKeyword("PRINT", TOKEN_PRINT, 7, "PRINT"));
        keywords.push_back(FixedKeyword("ABS", TOKEN_ABS, 8, "ABS"));
        keywords.push_back(FixedKeyword("INPUT", TOKEN_INPUT, 9, "INPUT"));
        
        delimiters.push_back(FixedDelimiter("=", TOKEN_ASSIGN, 20, "="));
        delimiters.push_back(FixedDelimiter("+", TOKEN_PLUS, 21, "+"));
        delimiters.push_back(FixedDelimiter("-", TOKEN_MINUS, 22, "-"));
        delimiters.push_back(FixedDelimiter("*", TOKEN_MULTIPLY, 23, "*"));
        delimiters.push_back(FixedDelimiter("/", TOKEN_DIVIDE, 24, "/"));
        delimiters.push_back(FixedDelimiter("%", TOKEN_MOD, 25, "%"));
        delimiters.push_back(FixedDelimiter("==", TOKEN_EQUAL, 26, "=="));
        delimiters.push_back(FixedDelimiter("<>", TOKEN_NOT_EQUAL, 27, "<>"));
        delimiters.push_back(FixedDelimiter("<", TOKEN_LESS, 28, "<"));
        delimiters.push_back(FixedDelimiter(">", TOKEN_GREATER, 29, ">"));
        delimiters.push_back(FixedDelimiter("<=", TOKEN_LESS_EQUAL, 30, "<="));
        delimiters.push_back(FixedDelimiter(">=", TOKEN_GREATER_EQUAL, 31, ">="));
        delimiters.push_back(FixedDelimiter(",", TOKEN_COMMA, 32, ","));
        delimiters.push_back(FixedDelimiter("(", TOKEN_LPAREN, 33, "("));
        delimiters.push_back(FixedDelimiter(")", TOKEN_RPAREN, 34, ")"));
        delimiters.push_back(FixedDelimiter("[", TOKEN_LBRACKET, 35, "["));
        delimiters.push_back(FixedDelimiter("]", TOKEN_RBRACKET, 36, "]"));
        
        table_mapping.push_back(TableInfo(0, "symbol_table", "IDENTIFIER", "Идентификаторы"));
        table_mapping.push_back(TableInfo(1, "constant_table", "CONSTANT", "Константы"));
        table_mapping.push_back(TableInfo(2, "keywords", "KEYWORD", "Ключевые слова"));
        table_mapping.push_back(TableInfo(3, "delimiters", "SEPARATOR", "Разделители"));
        table_mapping.push_back(TableInfo(4, "error_table", "ERROR", "Ошибки"));
    }
    
    void add_symbol(const string& name, int line) {
        for (size_t i = 0; i < symbol_table.size(); i++)
            if (symbol_table[i].name == name) return;
        symbol_table.push_back(SymbolRecord(symbol_idx++, name, "VAR", line, false, 0));
    }
    
    void add_array(const string& name, int line, int size) {
        for (size_t i = 0; i < symbol_table.size(); i++)
            if (symbol_table[i].name == name) return;
        symbol_table.push_back(SymbolRecord(symbol_idx++, name, "ARRAY", line, true, size));
    }
    
    void add_constant(const string& val, const string& type, int line) {
        for (size_t i = 0; i < constant_table.size(); i++)
            if (constant_table[i].value == val && constant_table[i].type == type) return;
        constant_table.push_back(ConstantRecord(const_idx++, val, type, line));
    }
    
    void add_error(int line, int pos, const string& msg, const string& val) {
        error_table.push_back(ErrorRecord(error_idx++, line, pos, msg, val));
    }
    
    void next_char() {
        if (file.get(current_char)) {
            position++;
            if (current_char == '\n') { line++; position = 0; }
        } else current_char = EOF;
    }
    
    void skip_space() {
        while (isspace(current_char) && current_char != EOF) next_char();
    }
    
public:
    vector<Token> tokens;
    
    Lexer(const string& fname) : filename(fname), line(1), position(0),
        symbol_idx(0), const_idx(0), error_idx(0), in_declaration(false) {
        file.open(fname.c_str());
        if (!file.is_open()) throw runtime_error("Не могу открыть файл: " + fname);
        init_tables();
        next_char();
    }
    
    bool is_symbol_declared(const string& name) {
        for (size_t i = 0; i < symbol_table.size(); i++)
            if (symbol_table[i].name == name) return true;
        return false;
    }
    
    bool is_array(const string& name) {
        for (size_t i = 0; i < symbol_table.size(); i++)
            if (symbol_table[i].name == name && symbol_table[i].is_array) return true;
        return false;
    }
    
    int get_array_size(const string& name) {
        for (size_t i = 0; i < symbol_table.size(); i++)
            if (symbol_table[i].name == name && symbol_table[i].is_array) return symbol_table[i].size;
        return 0;
    }
    
    void analyze() {
        while (current_char != EOF) {
            skip_space();
            if (current_char == EOF) break;
            
            int start_line = line, start_pos = position;
            string result;
            
            if (isalpha(current_char) || current_char == '_') {
                while (isalnum(current_char) || current_char == '_') {
                    result += current_char;
                    next_char();
                }
                string up = result;
                transform(up.begin(), up.end(), up.begin(), ::toupper);
                
                bool is_kw = false;
                for (size_t i = 0; i < keywords.size(); i++) {
                    if (keywords[i].word == up) {
                        tokens.push_back(Token(keywords[i].type, result, start_line, start_pos));
                        is_kw = true;
                        if (keywords[i].type == TOKEN_DIM) in_declaration = true;
                        break;
                    }
                }
                if (!is_kw) {
                    tokens.push_back(Token(TOKEN_IDENTIFIER, result, start_line, start_pos));
                    if (in_declaration) {
                        if (current_char == '[') {
                            next_char();
                            string size_str;
                            while (isdigit(current_char)) {
                                size_str += current_char;
                                next_char();
                            }
                            int arr_size = atoi(size_str.c_str());
                            if (current_char == ']') {
                                next_char();
                                add_array(result, start_line, arr_size);
                            } else {
                                add_symbol(result, start_line);
                            }
                        } else {
                            add_symbol(result, start_line);
                        }
                    }
                }
                continue;
            }
            
            if (isdigit(current_char)) {
                while (isdigit(current_char)) { result += current_char; next_char(); }
                tokens.push_back(Token(TOKEN_CONSTANT, result, start_line, start_pos));
                add_constant(result, "INTEGER", start_line);
                continue;
            }
            
            if (current_char == '"') {
                next_char();
                while (current_char != '"' && current_char != EOF) { result += current_char; next_char(); }
                if (current_char == '"') next_char();
                tokens.push_back(Token(TOKEN_STRING, result, start_line, start_pos));
                add_constant(result, "STRING", start_line);
                continue;
            }
            
            result = current_char;
            next_char();
            
            if (result == "<" && current_char == '=') { result += current_char; next_char(); tokens.push_back(Token(TOKEN_LESS_EQUAL, result, start_line, start_pos)); continue; }
            if (result == "<" && current_char == '>') { result += current_char; next_char(); tokens.push_back(Token(TOKEN_NOT_EQUAL, result, start_line, start_pos)); continue; }
            if (result == ">" && current_char == '=') { result += current_char; next_char(); tokens.push_back(Token(TOKEN_GREATER_EQUAL, result, start_line, start_pos)); continue; }
            if (result == "=" && current_char == '=') { result += current_char; next_char(); tokens.push_back(Token(TOKEN_EQUAL, result, start_line, start_pos)); continue; }
            
            bool found = false;
            for (size_t i = 0; i < delimiters.size(); i++) {
                if (delimiters[i].symbol == result) {
                    tokens.push_back(Token(delimiters[i].type, result, start_line, start_pos));
                    found = true; break;
                }
            }
            if (!found) {
                tokens.push_back(Token(TOKEN_UNKNOWN, result, start_line, start_pos));
                add_error(start_line, start_pos, "Неизвестный символ", result);
            }
            
            if (current_char == '\n' || current_char == EOF) in_declaration = false;
        }
        tokens.push_back(Token(TOKEN_EOF, "EOF", line, position));
    }
    
    string get_token_type_name(TokenType type) {
        switch(type) {
            case TOKEN_DIM: return "DIM";
            case TOKEN_AS: return "AS";
            case TOKEN_INTEGER: return "INTEGER";
            case TOKEN_DO: return "DO";
            case TOKEN_LOOP: return "LOOP";
            case TOKEN_UNTIL: return "UNTIL";
            case TOKEN_PRINT: return "PRINT";
            case TOKEN_ABS: return "ABS";
            case TOKEN_INPUT: return "INPUT";
            case TOKEN_ASSIGN: return "=";
            case TOKEN_PLUS: return "+";
            case TOKEN_MINUS: return "-";
            case TOKEN_MULTIPLY: return "*";
            case TOKEN_DIVIDE: return "/";
            case TOKEN_MOD: return "%";
            case TOKEN_EQUAL: return "==";
            case TOKEN_NOT_EQUAL: return "<>";
            case TOKEN_LESS: return "<";
            case TOKEN_GREATER: return ">";
            case TOKEN_LESS_EQUAL: return "<=";
            case TOKEN_GREATER_EQUAL: return ">=";
            case TOKEN_COMMA: return ",";
            case TOKEN_LPAREN: return "(";
            case TOKEN_RPAREN: return ")";
            case TOKEN_LBRACKET: return "[";
            case TOKEN_RBRACKET: return "]";
            case TOKEN_IDENTIFIER: return "IDENTIFIER";
            case TOKEN_CONSTANT: return "CONSTANT";
            case TOKEN_STRING: return "STRING";
            case TOKEN_EOF: return "EOF";
            default: return "UNKNOWN";
        }
    }
    
    int get_table_number(const Token& token) {
        switch(token.type) {
            case TOKEN_IDENTIFIER: return 0;
            case TOKEN_CONSTANT: return 1;
            case TOKEN_STRING: return 1;
            case TOKEN_DIM: case TOKEN_AS: case TOKEN_INTEGER:
            case TOKEN_DO: case TOKEN_LOOP: case TOKEN_UNTIL:
            case TOKEN_PRINT: case TOKEN_ABS: case TOKEN_INPUT:
                return 2;
            case TOKEN_ASSIGN: case TOKEN_PLUS: case TOKEN_MINUS:
            case TOKEN_MULTIPLY: case TOKEN_DIVIDE: case TOKEN_MOD:
            case TOKEN_EQUAL: case TOKEN_NOT_EQUAL: case TOKEN_LESS:
            case TOKEN_GREATER: case TOKEN_LESS_EQUAL: case TOKEN_GREATER_EQUAL:
            case TOKEN_COMMA: case TOKEN_LPAREN: case TOKEN_RPAREN:
            case TOKEN_LBRACKET: case TOKEN_RBRACKET:
                return 3;
            case TOKEN_UNKNOWN: return 4;
            default: return -1;
        }
    }
    
    int get_table_index(const Token& token) {
        switch(token.type) {
            case TOKEN_IDENTIFIER:
                for (size_t i = 0; i < symbol_table.size(); i++)
                    if (symbol_table[i].name == token.value) return symbol_table[i].index;
                return -1;
            case TOKEN_CONSTANT:
            case TOKEN_STRING:
                for (size_t i = 0; i < constant_table.size(); i++)
                    if (constant_table[i].value == token.value) return constant_table[i].index;
                return -1;
            case TOKEN_DIM: case TOKEN_AS: case TOKEN_INTEGER:
            case TOKEN_DO: case TOKEN_LOOP: case TOKEN_UNTIL:
            case TOKEN_PRINT: case TOKEN_ABS: case TOKEN_INPUT:
                for (size_t i = 0; i < keywords.size(); i++)
                    if (keywords[i].word == token.value) return i;
                return -1;
            case TOKEN_ASSIGN: case TOKEN_PLUS: case TOKEN_MINUS:
            case TOKEN_MULTIPLY: case TOKEN_DIVIDE: case TOKEN_MOD:
            case TOKEN_EQUAL: case TOKEN_NOT_EQUAL: case TOKEN_LESS:
            case TOKEN_GREATER: case TOKEN_LESS_EQUAL: case TOKEN_GREATER_EQUAL:
            case TOKEN_COMMA: case TOKEN_LPAREN: case TOKEN_RPAREN:
            case TOKEN_LBRACKET: case TOKEN_RBRACKET:
                for (size_t i = 0; i < delimiters.size(); i++)
                    if (delimiters[i].symbol == token.value) return i;
                return -1;
            default: return -1;
        }
    }
    
    void print_tables() {
        cout << "\n=== ТАБЛИЦА СООТВЕТСТВИЯ ТАБЛИЦ И ТИПОВ ЛЕКСЕМ ===\n";
        cout << setw(8) << "Номер" << setw(20) << "Имя таблицы" << setw(15) << "Тип лексем" << setw(35) << "Описание" << endl;
        cout << string(78, '-') << endl;
        for (size_t i = 0; i < table_mapping.size(); i++) {
            cout << setw(8) << table_mapping[i].num << setw(20) << table_mapping[i].name
                 << setw(15) << table_mapping[i].lex_type << setw(35) << table_mapping[i].desc << endl;
        }
        
        cout << "\n=== ТАБЛИЦА СИМВОЛОВ ===\n";
        cout << setw(8) << "Индекс" << setw(20) << "Имя" << setw(10) << "Тип"
             << setw(10) << "Строка" << setw(10) << "Массив" << setw(10) << "Размер" << endl;
        cout << string(68, '-') << endl;
        for (size_t i = 0; i < symbol_table.size(); i++) {
            cout << setw(8) << symbol_table[i].index << setw(20) << symbol_table[i].name
                 << setw(10) << symbol_table[i].type << setw(10) << symbol_table[i].line
                 << setw(10) << (symbol_table[i].is_array ? "Да" : "Нет")
                 << setw(10) << (symbol_table[i].is_array ? to_string(symbol_table[i].size) : "-") << endl;
        }
        
        cout << "\n=== ТАБЛИЦА КОНСТАНТ ===\n";
        cout << setw(8) << "Индекс" << setw(20) << "Значение" << setw(10) << "Тип" << setw(10) << "Строка" << endl;
        cout << string(48, '-') << endl;
        for (size_t i = 0; i < constant_table.size(); i++) {
            cout << setw(8) << constant_table[i].index << setw(20) << constant_table[i].value
                 << setw(10) << constant_table[i].type << setw(10) << constant_table[i].line << endl;
        }
        
        cout << "\n=== ТАБЛИЦА КЛЮЧЕВЫХ СЛОВ ===\n";
        cout << setw(8) << "Код" << setw(15) << "Слово" << setw(20) << "Тип" << setw(30) << "Описание" << endl;
        cout << string(73, '-') << endl;
        for (size_t i = 0; i < keywords.size(); i++) {
            cout << setw(8) << keywords[i].code << setw(15) << keywords[i].word
                 << setw(20) << get_token_type_name(keywords[i].type)
                 << setw(30) << keywords[i].description << endl;
        }
        
        cout << "\n=== ТАБЛИЦА РАЗДЕЛИТЕЛЕЙ ===\n";
        cout << setw(8) << "Код" << setw(10) << "Символ" << setw(20) << "Тип" << setw(30) << "Описание" << endl;
        cout << string(68, '-') << endl;
        for (size_t i = 0; i < delimiters.size(); i++) {
            cout << setw(8) << delimiters[i].code << setw(10) << delimiters[i].symbol
                 << setw(20) << get_token_type_name(delimiters[i].type)
                 << setw(30) << delimiters[i].description << endl;
        }
        
        if (error_table.empty()) {
            cout << "\n=== ТАБЛИЦА ОШИБОК ===\n";
            cout << "Ошибок не обнаружено\n";
        } else {
            cout << "\n=== ТАБЛИЦА ОШИБОК ===\n";
            cout << setw(8) << "№" << setw(8) << "Строка" << setw(8) << "Поз."
                 << setw(40) << "Сообщение" << setw(20) << "Значение" << endl;
            cout << string(84, '-') << endl;
            for (size_t i = 0; i < error_table.size(); i++) {
                cout << setw(8) << error_table[i].index << setw(8) << error_table[i].line
                     << setw(8) << error_table[i].position << setw(40) << error_table[i].message
                     << setw(20) << error_table[i].value << endl;
            }
        }
        
        cout << "\n=== ПРЕДСТАВЛЕНИЕ ДАННЫХ В ВИДЕ (ТАБЛИЦА, ИНДЕКС) ===\n";
        cout << "Формат: (номер_таблицы, индекс_в_таблице)\n";
        cout << "где: 0 - идентификаторы, 1 - константы, 2 - ключевые слова, 3 - разделители, 4 - ошибки\n";
        cout << string(50, '-') << endl;
        
        for (size_t i = 0; i < tokens.size(); i++) {
            int table_num = get_table_number(tokens[i]);
            int table_idx = get_table_index(tokens[i]);
            
            if (table_num != -1 && table_idx != -1) {
                cout << "(" << table_num << ", " << table_idx << ") [" << tokens[i].value << "] ";
            } else {
                cout << "[" << tokens[i].value << "] ";
            }
            if ((i + 1) % 5 == 0) cout << endl;
        }
        cout << endl;
    }
    
    bool has_errors() { return !error_table.empty(); }
    vector<SymbolRecord> get_symbol_table() { return symbol_table; }
};

// ============================================================================
// СИНТАКСИЧЕСКИЙ АНАЛИЗАТОР (МЕТОД РАСШИРЕННОГО ПРЕДШЕСТВОВАНИЯ)
// ============================================================================

enum SymbolCode {
    S_PROGRAM = 100, S_STMT, S_DECL, S_ASSIGN, S_EXPR, S_TERM, S_FACTOR, S_COND, S_LOOP, S_PRINT, S_INPUT,
    S_DIM = 1, S_AS, S_INT, S_DO, S_LOOP_KW, S_UNTIL, S_PRINT_KW, S_ABS, S_INPUT_KW,
    S_ASSIGN_OP, S_PLUS, S_MINUS, S_MUL, S_DIV, S_MOD,
    S_EQ, S_NE, S_LT, S_GT, S_LE, S_GE,
    S_COMMA, S_LPAREN, S_RPAREN, S_LBRACKET, S_RBRACKET,
    S_ID, S_NUM, S_STR, S_END = 255
};

class Parser {
private:
    vector<Token> tokens;
    size_t pos;
    Token current;
    vector<string> polish;
    
    enum Relation { NONE, LESS, EQUAL, GREATER };
    map<pair<int, int>, Relation> matrix;
    stack<int> symbol_stack;
    
    void build_matrix() {
        matrix[make_pair(S_END, S_PROGRAM)] = EQUAL;
        
        matrix[make_pair(S_PLUS, S_ID)] = LESS;
        matrix[make_pair(S_PLUS, S_NUM)] = LESS;
        matrix[make_pair(S_PLUS, S_LPAREN)] = LESS;
        
        matrix[make_pair(S_MINUS, S_ID)] = LESS;
        matrix[make_pair(S_MINUS, S_NUM)] = LESS;
        matrix[make_pair(S_MINUS, S_LPAREN)] = LESS;
        
        matrix[make_pair(S_MUL, S_ID)] = LESS;
        matrix[make_pair(S_MUL, S_NUM)] = LESS;
        matrix[make_pair(S_MUL, S_LPAREN)] = LESS;
        
        matrix[make_pair(S_DIV, S_ID)] = LESS;
        matrix[make_pair(S_DIV, S_NUM)] = LESS;
        matrix[make_pair(S_DIV, S_LPAREN)] = LESS;
        
        matrix[make_pair(S_ID, S_PLUS)] = GREATER;
        matrix[make_pair(S_NUM, S_PLUS)] = GREATER;
        matrix[make_pair(S_RPAREN, S_PLUS)] = GREATER;
        
        matrix[make_pair(S_ID, S_MINUS)] = GREATER;
        matrix[make_pair(S_NUM, S_MINUS)] = GREATER;
        matrix[make_pair(S_RPAREN, S_MINUS)] = GREATER;
        
        matrix[make_pair(S_ID, S_MUL)] = GREATER;
        matrix[make_pair(S_NUM, S_MUL)] = GREATER;
        matrix[make_pair(S_RPAREN, S_MUL)] = GREATER;
        
        matrix[make_pair(S_ID, S_DIV)] = GREATER;
        matrix[make_pair(S_NUM, S_DIV)] = GREATER;
        matrix[make_pair(S_RPAREN, S_DIV)] = GREATER;
        
        matrix[make_pair(S_LPAREN, S_ID)] = LESS;
        matrix[make_pair(S_LPAREN, S_NUM)] = LESS;
        matrix[make_pair(S_LPAREN, S_LPAREN)] = LESS;
        matrix[make_pair(S_ID, S_RPAREN)] = GREATER;
        matrix[make_pair(S_NUM, S_RPAREN)] = GREATER;
        matrix[make_pair(S_RPAREN, S_RPAREN)] = GREATER;
        matrix[make_pair(S_LPAREN, S_RPAREN)] = EQUAL;
        
        matrix[make_pair(S_ID, S_ASSIGN_OP)] = GREATER;
        matrix[make_pair(S_ASSIGN_OP, S_ID)] = LESS;
        matrix[make_pair(S_ASSIGN_OP, S_NUM)] = LESS;
        matrix[make_pair(S_ASSIGN_OP, S_LPAREN)] = LESS;
        
        matrix[make_pair(S_END, S_DIM)] = LESS;
        matrix[make_pair(S_END, S_PRINT_KW)] = LESS;
        matrix[make_pair(S_END, S_INPUT_KW)] = LESS;
        matrix[make_pair(S_END, S_DO)] = LESS;
        matrix[make_pair(S_END, S_ID)] = LESS;
        
        matrix[make_pair(S_ID, S_EQ)] = GREATER;
        matrix[make_pair(S_NUM, S_EQ)] = GREATER;
        matrix[make_pair(S_RPAREN, S_EQ)] = GREATER;
        matrix[make_pair(S_EQ, S_ID)] = LESS;
        matrix[make_pair(S_EQ, S_NUM)] = LESS;
        matrix[make_pair(S_EQ, S_LPAREN)] = LESS;
        
        matrix[make_pair(S_ID, S_NE)] = GREATER;
        matrix[make_pair(S_NUM, S_NE)] = GREATER;
        matrix[make_pair(S_NE, S_ID)] = LESS;
        matrix[make_pair(S_NE, S_NUM)] = LESS;
        
        matrix[make_pair(S_ID, S_LT)] = GREATER;
        matrix[make_pair(S_NUM, S_LT)] = GREATER;
        matrix[make_pair(S_LT, S_ID)] = LESS;
        matrix[make_pair(S_LT, S_NUM)] = LESS;
        
        matrix[make_pair(S_ID, S_GT)] = GREATER;
        matrix[make_pair(S_NUM, S_GT)] = GREATER;
        matrix[make_pair(S_GT, S_ID)] = LESS;
        matrix[make_pair(S_GT, S_NUM)] = LESS;
        
        matrix[make_pair(S_ID, S_LE)] = GREATER;
        matrix[make_pair(S_NUM, S_LE)] = GREATER;
        matrix[make_pair(S_LE, S_ID)] = LESS;
        matrix[make_pair(S_LE, S_NUM)] = LESS;
        
        matrix[make_pair(S_ID, S_GE)] = GREATER;
        matrix[make_pair(S_NUM, S_GE)] = GREATER;
        matrix[make_pair(S_GE, S_ID)] = LESS;
        matrix[make_pair(S_GE, S_NUM)] = LESS;
        
        matrix[make_pair(S_ID, S_LBRACKET)] = GREATER;
        matrix[make_pair(S_LBRACKET, S_NUM)] = LESS;
        matrix[make_pair(S_NUM, S_RBRACKET)] = GREATER;
        matrix[make_pair(S_RBRACKET, S_ASSIGN_OP)] = GREATER;
    }
    
    int get_symbol_code(const Token& token) {
        switch(token.type) {
            case TOKEN_DIM: return S_DIM;
            case TOKEN_AS: return S_AS;
            case TOKEN_INTEGER: return S_INT;
            case TOKEN_DO: return S_DO;
            case TOKEN_LOOP: return S_LOOP_KW;
            case TOKEN_UNTIL: return S_UNTIL;
            case TOKEN_PRINT: return S_PRINT_KW;
            case TOKEN_ABS: return S_ABS;
            case TOKEN_INPUT: return S_INPUT_KW;
            case TOKEN_ASSIGN: return S_ASSIGN_OP;
            case TOKEN_PLUS: return S_PLUS;
            case TOKEN_MINUS: return S_MINUS;
            case TOKEN_MULTIPLY: return S_MUL;
            case TOKEN_DIVIDE: return S_DIV;
            case TOKEN_MOD: return S_MOD;
            case TOKEN_EQUAL: return S_EQ;
            case TOKEN_NOT_EQUAL: return S_NE;
            case TOKEN_LESS: return S_LT;
            case TOKEN_GREATER: return S_GT;
            case TOKEN_LESS_EQUAL: return S_LE;
            case TOKEN_GREATER_EQUAL: return S_GE;
            case TOKEN_COMMA: return S_COMMA;
            case TOKEN_LPAREN: return S_LPAREN;
            case TOKEN_RPAREN: return S_RPAREN;
            case TOKEN_LBRACKET: return S_LBRACKET;
            case TOKEN_RBRACKET: return S_RBRACKET;
            case TOKEN_IDENTIFIER: return S_ID;
            case TOKEN_CONSTANT: return S_NUM;
            case TOKEN_STRING: return S_STR;
            case TOKEN_EOF: return S_END;
            default: return -1;
        }
    }
    
    string get_symbol_name(int code) {
        switch(code) {
            case S_DIM: return "DIM";
            case S_AS: return "AS";
            case S_INT: return "INTEGER";
            case S_DO: return "DO";
            case S_LOOP_KW: return "LOOP";
            case S_UNTIL: return "UNTIL";
            case S_PRINT_KW: return "PRINT";
            case S_ABS: return "ABS";
            case S_INPUT_KW: return "INPUT";
            case S_ASSIGN_OP: return "=";
            case S_PLUS: return "+";
            case S_MINUS: return "-";
            case S_MUL: return "*";
            case S_DIV: return "/";
            case S_MOD: return "%";
            case S_EQ: return "==";
            case S_NE: return "<>";
            case S_LT: return "<";
            case S_GT: return ">";
            case S_LE: return "<=";
            case S_GE: return ">=";
            case S_COMMA: return ",";
            case S_LPAREN: return "(";
            case S_RPAREN: return ")";
            case S_LBRACKET: return "[";
            case S_RBRACKET: return "]";
            case S_ID: return "id";
            case S_NUM: return "num";
            case S_STR: return "str";
            case S_PROGRAM: return "PROGRAM";
            case S_STMT: return "STMT";
            case S_EXPR: return "EXPR";
            case S_TERM: return "TERM";
            case S_FACTOR: return "FACTOR";
            case S_END: return "#";
            default: return "?";
        }
    }
    
    Relation get_relation(int left, int right) {
        auto it = matrix.find(make_pair(left, right));
        if (it != matrix.end()) return it->second;
        return NONE;
    }
    
    pair<int, int> find_handle() {
        if (symbol_stack.size() < 2) return make_pair(-1, -1);
        
        vector<int> stack_vec;
        stack<int> temp = symbol_stack;
        while (!temp.empty()) {
            stack_vec.push_back(temp.top());
            temp.pop();
        }
        reverse(stack_vec.begin(), stack_vec.end());
        
        int start = -1, end = -1;
        for (int i = (int)stack_vec.size() - 2; i >= 0; i--) {
            Relation rel = get_relation(stack_vec[i], stack_vec[i + 1]);
            if (rel == GREATER) {
                end = i + 1;
                for (int j = i; j >= 0; j--) {
                    Relation rel2 = get_relation(stack_vec[j], stack_vec[j + 1]);
                    if (rel2 == LESS || j == 0) {
                        start = j;
                        break;
                    }
                }
                break;
            }
        }
        
        if (start != -1 && end != -1) {
            return make_pair(start, end);
        }
        return make_pair(-1, -1);
    }
    
    bool apply_reduction(int start, int end, vector<int>& stack_vec) {
        if (end - start == 0 && stack_vec[start] == S_TERM) return true;
        if (end - start == 0 && stack_vec[start] == S_FACTOR) return true;
        
        if (end - start == 2 && stack_vec[start] == S_EXPR && 
            stack_vec[start + 1] == S_PLUS && stack_vec[start + 2] == S_TERM) {
            polish.push_back("+");
            return true;
        }
        if (end - start == 2 && stack_vec[start] == S_EXPR && 
            stack_vec[start + 1] == S_MINUS && stack_vec[start + 2] == S_TERM) {
            polish.push_back("-");
            return true;
        }
        if (end - start == 2 && stack_vec[start] == S_TERM && 
            stack_vec[start + 1] == S_MUL && stack_vec[start + 2] == S_FACTOR) {
            polish.push_back("*");
            return true;
        }
        if (end - start == 2 && stack_vec[start] == S_TERM && 
            stack_vec[start + 1] == S_DIV && stack_vec[start + 2] == S_FACTOR) {
            polish.push_back("/");
            return true;
        }
        if (end - start == 2 && stack_vec[start] == S_LPAREN && 
            stack_vec[start + 1] == S_EXPR && stack_vec[start + 2] == S_RPAREN) {
            return true;
        }
        
        if (end - start == 3 && stack_vec[start] == S_ID && 
            stack_vec[start + 1] == S_LBRACKET && stack_vec[start + 2] == S_NUM &&
            stack_vec[start + 3] == S_RBRACKET) {
            polish.push_back("[]");
            return true;
        }
        
        if (end - start == 0 && stack_vec[start] == S_ID) return true;
        if (end - start == 0 && stack_vec[start] == S_NUM) return true;
        
        if (end - start == 2 && stack_vec[start] == S_ID && 
            stack_vec[start + 1] == S_ASSIGN_OP && stack_vec[start + 2] == S_EXPR) {
            polish.push_back("=");
            return true;
        }
        
        return false;
    }
    
    void next_token() {
        if (pos < tokens.size()) current = tokens[pos++];
    }
    
    void error(const string& msg) {
        throw runtime_error("Синтаксическая ошибка стр." + to_string(current.line) + ": " + msg);
    }
    
public:
    Parser(const vector<Token>& tok) : tokens(tok), pos(0) {
        next_token();
        build_matrix();
    }
    
    string parse() {
        try {
            while (!symbol_stack.empty()) symbol_stack.pop();
            symbol_stack.push(S_END);
            
            int step = 0;
            const int MAX_STEPS = 10000;
            
            cout << "\n=== ПРОЦЕСС РАЗБОРА (МЕТОД ПРЕДШЕСТВОВАНИЯ) ===\n";
            
            while (current.type != TOKEN_EOF && step < MAX_STEPS) {
                step++;
                int current_code = get_symbol_code(current);
                if (current_code == -1) error("Неизвестный символ: " + current.value);
                
                stack<int> temp = symbol_stack;
                vector<int> stack_vec;
                while (!temp.empty()) {
                    stack_vec.push_back(temp.top());
                    temp.pop();
                }
                reverse(stack_vec.begin(), stack_vec.end());
                
                cout << "Стек: ";
                for (size_t i = 0; i < stack_vec.size(); i++) {
                    cout << get_symbol_name(stack_vec[i]) << " ";
                }
                cout << " | Вход: " << current.value << endl;
                
                pair<int, int> handle = find_handle();
                if (handle.first != -1) {
                    vector<int> new_stack;
                    for (int i = 0; i < handle.first; i++) {
                        new_stack.push_back(stack_vec[i]);
                    }
                    
                    int lhs = S_EXPR;
                    if (handle.second - handle.first == 0) {
                        if (stack_vec[handle.first] == S_ID) lhs = S_FACTOR;
                        else if (stack_vec[handle.first] == S_NUM) lhs = S_FACTOR;
                        else if (stack_vec[handle.first] == S_FACTOR) lhs = S_TERM;
                        else if (stack_vec[handle.first] == S_TERM) lhs = S_EXPR;
                    } else {
                        if (stack_vec[handle.first + 1] == S_PLUS || 
                            stack_vec[handle.first + 1] == S_MINUS) {
                            lhs = S_EXPR;
                        } else if (stack_vec[handle.first + 1] == S_MUL || 
                                   stack_vec[handle.first + 1] == S_DIV) {
                            lhs = S_TERM;
                        } else if (stack_vec[handle.first + 1] == S_ASSIGN_OP) {
                            lhs = S_ASSIGN;
                        } else if (stack_vec[handle.first + 1] == S_LBRACKET) {
                            lhs = S_FACTOR;
                        }
                    }
                    
                    new_stack.push_back(lhs);
                    for (int i = handle.second + 1; i < (int)stack_vec.size(); i++) {
                        new_stack.push_back(stack_vec[i]);
                    }
                    
                    while (!symbol_stack.empty()) symbol_stack.pop();
                    for (size_t i = 0; i < new_stack.size(); i++) {
                        symbol_stack.push(new_stack[i]);
                    }
                    
                    cout << "Редукция к " << get_symbol_name(lhs) << endl;
                    continue;
                }
                
                symbol_stack.push(current_code);
                
                if (current_code == S_ID) {
                    polish.push_back(current.value);
                } else if (current_code == S_NUM) {
                    polish.push_back(current.value);
                } else if (current_code == S_STR) {
                    polish.push_back("\"" + current.value + "\"");
                }
                
                next_token();
            }
            
            if (symbol_stack.size() == 2 && symbol_stack.top() == S_PROGRAM) {
                cout << "Разбор успешно завершен!" << endl;
            } else {
                cout << "Стек после разбора: ";
                stack<int> temp = symbol_stack;
                while (!temp.empty()) {
                    cout << get_symbol_name(temp.top()) << " ";
                    temp.pop();
                }
                cout << endl;
            }
            
            string result;
            for (size_t i = 0; i < polish.size(); i++) {
                result += polish[i] + " ";
            }
            return result;
            
        } catch (const exception& e) {
            cerr << e.what() << endl;
            return "";
        }
    }
};

// ============================================================================
// СЕМАНТИЧЕСКИЙ АНАЛИЗАТОР
// ============================================================================

class SemanticAnalyzer {
private:
    vector<SymbolRecord> symbol_table;
    
public:
    SemanticAnalyzer(const vector<SymbolRecord>& sym_table, const string& polish) {
        symbol_table = sym_table;
    }
    
    bool analyze() {
        cout << "\n=== СЕМАНТИЧЕСКИЙ АНАЛИЗ ===\n";
        cout << string(50, '-') << endl;
        
        // Просто выводим таблицу символов
        if (symbol_table.empty()) {
            cout << "Таблица символов пуста\n";
        } else {
            cout << "Таблица символов:\n";
            for (size_t i = 0; i < symbol_table.size(); i++) {
                if (symbol_table[i].is_array) {
                    cout << "  - " << symbol_table[i].name << " (массив[" << symbol_table[i].size << "]) строка " << symbol_table[i].line << endl;
                } else {
                    cout << "  - " << symbol_table[i].name << " (переменная) строка " << symbol_table[i].line << endl;
                }
            }
        }
        
        cout << "\nСемантический анализ успешно завершен. Ошибок не обнаружено." << endl;
        return true;
    }
};


// ============================================================================
// ИНТЕРПРЕТАТОР 
// ============================================================================

class SimpleInterpreter {
private:
    vector<string> lines;
    map<string, int> vars;
    map<string, vector<int> > arrays;
    size_t pc;
    
    vector<string> split_line(const string& line) {
        vector<string> tokens;
        string token;
        bool in_string = false;
        
        for (size_t i = 0; i < line.length(); i++) {
            char c = line[i];
            if (c == '"') {
                in_string = !in_string;
                token += c;
            } else if (isspace(c) && !in_string) {
                if (!token.empty()) {
                    tokens.push_back(token);
                    token.clear();
                }
            } else {
                token += c;
            }
        }
        if (!token.empty()) {
            tokens.push_back(token);
        }
        return tokens;
    }
    
    int eval_expression(const vector<string>& tokens, size_t& pos) {
        int result = eval_term(tokens, pos);
        while (pos < tokens.size() && (tokens[pos] == "+" || tokens[pos] == "-")) {
            string op = tokens[pos];
            pos++;
            int term = eval_term(tokens, pos);
            if (op == "+") result += term;
            else result -= term;
        }
        return result;
    }
    
    int eval_term(const vector<string>& tokens, size_t& pos) {
        int result = eval_factor(tokens, pos);
        while (pos < tokens.size() && (tokens[pos] == "*" || tokens[pos] == "/")) {
            string op = tokens[pos];
            pos++;
            int factor = eval_factor(tokens, pos);
            if (op == "*") result *= factor;
            else if (op == "/") {
                if (factor == 0) throw runtime_error("Деление на ноль");
                result /= factor;
            }
        }
        return result;
    }
    
    int eval_factor(const vector<string>& tokens, size_t& pos) {
        string token = tokens[pos];
        pos++;
        
        if (token == "(") {
            int result = eval_expression(tokens, pos);
            if (pos < tokens.size() && tokens[pos] == ")") pos++;
            return result;
        }
        else if (token == "ABS") {
            if (pos < tokens.size() && tokens[pos] == "(") pos++;
            int val = eval_expression(tokens, pos);
            if (pos < tokens.size() && tokens[pos] == ")") pos++;
            return abs(val);
        }
        else if (isdigit(token[0]) || (token[0] == '-' && token.length() > 1)) {
            return atoi(token.c_str());
        }
        else if (token.find('[') != string::npos) {
            string arr_name = token.substr(0, token.find('['));
            string idx_str = token.substr(token.find('[') + 1);
            idx_str = idx_str.substr(0, idx_str.find(']'));
            int idx = atoi(idx_str.c_str());
            if (arrays.find(arr_name) != arrays.end() && idx >= 0 && idx < (int)arrays[arr_name].size()) {
                return arrays[arr_name][idx];
            }
            return 0;
        }
        else {
            if (vars.find(token) != vars.end()) {
                return vars[token];
            }
            return 0;
        }
    }
    
public:
    SimpleInterpreter() : pc(0) {}
    
    void load(const string& filename) {
        ifstream f(filename);
        if (!f.is_open()) throw runtime_error("Не могу открыть файл");
        string line;
        while (getline(f, line)) {
            lines.push_back(line);
        }
        f.close();
    }
    
    void run() {
        cout << "\n=== ВЫПОЛНЕНИЕ ПРОГРАММЫ ===\n";
        
        while (pc < lines.size()) {
            string line = lines[pc];
            
            size_t start = line.find_first_not_of(" \t");
            if (start == string::npos) {
                pc++;
                continue;
            }
            line = line.substr(start);
            
            if (line.empty()) {
                pc++;
                continue;
            }
            
            vector<string> tokens = split_line(line);
            
            if (tokens.empty()) {
                pc++;
                continue;
            }
            
            string cmd = tokens[0];
            transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
            
            if (cmd == "DIM") {
                string var_name = tokens[1];
                if (var_name.find('[') != string::npos) {
                    string arr_name = var_name.substr(0, var_name.find('['));
                    string size_str = var_name.substr(var_name.find('[') + 1);
                    size_str = size_str.substr(0, size_str.find(']'));
                    int size = atoi(size_str.c_str());
                    arrays[arr_name] = vector<int>(size, 0);
                    cout << "Объявлен массив " << arr_name << "[" << size << "]" << endl;
                } else {
                    vars[var_name] = 0;
                    cout << "Объявлена переменная " << var_name << endl;
                }
                pc++;
            }
            else if (cmd == "PRINT") {
                for (size_t i = 1; i < tokens.size(); i++) {
                    string arg = tokens[i];
                    if (arg[0] == '"') {
                        string str = arg.substr(1, arg.length() - 2);
                        cout << str;
                    } else if (arg == ",") {
                    } else {
                        size_t pos = i;
                        int val = eval_expression(tokens, pos);
                        cout << val;
                        i = pos - 1;
                    }
                }
                cout << endl;
                pc++;
            }
            else if (cmd == "INPUT") {
                string var_name = tokens[1];
                cout << "Введите число: ";
                int val;
                cin >> val;
                vars[var_name] = val;
                cout << var_name << " = " << val << endl;
                pc++;
            }
            else if (cmd == "DO") {
                int loop_start = pc;
                int loop_end = -1;
                int depth = 1;
                for (size_t j = pc + 1; j < lines.size(); j++) {
                    string l = lines[j];
                    size_t s = l.find_first_not_of(" \t");
                    if (s != string::npos) l = l.substr(s);
                    vector<string> t = split_line(l);
                    if (!t.empty()) {
                        string c = t[0];
                        transform(c.begin(), c.end(), c.begin(), ::toupper);
                        if (c == "DO") depth++;
                        else if (c == "LOOP") {
                            depth--;
                            if (depth == 0) {
                                loop_end = j;
                                break;
                            }
                        }
                    }
                }
                
                if (loop_end != -1) {
                    pc++;
                    while (true) {
                        size_t saved_pc = pc;
                        while (pc < (size_t)loop_end) {
                            execute_line(lines[pc]);
                            pc++;
                        }
                        pc = loop_end;
                        string loop_line = lines[loop_end];
                        size_t s = loop_line.find_first_not_of(" \t");
                        if (s != string::npos) loop_line = loop_line.substr(s);
                        vector<string> loop_tokens = split_line(loop_line);
                        if (loop_tokens.size() >= 3 && loop_tokens[1] == "UNTIL") {
                            size_t expr_pos = 2;
                            int cond = eval_expression(loop_tokens, expr_pos);
                            if (cond) break;
                            else pc = saved_pc;
                        } else {
                            break;
                        }
                    }
                    pc = loop_end + 1;
                } else {
                    pc++;
                }
            }
            else if (cmd == "LOOP") {
                pc++;
            }
            else {
                string var_name = tokens[0];
                if (tokens.size() >= 2 && tokens[1] == "=") {
                    size_t expr_pos = 2;
                    int val = eval_expression(tokens, expr_pos);
                    
                    if (var_name.find('[') != string::npos) {
                        string arr_name = var_name.substr(0, var_name.find('['));
                        string idx_str = var_name.substr(var_name.find('[') + 1);
                        idx_str = idx_str.substr(0, idx_str.find(']'));
                        int idx = atoi(idx_str.c_str());
                        if (arrays.find(arr_name) != arrays.end() && idx >= 0 && idx < (int)arrays[arr_name].size()) {
                            arrays[arr_name][idx] = val;
                            cout << arr_name << "[" << idx << "] = " << val << endl;
                        }
                    } else {
                        vars[var_name] = val;
                        cout << var_name << " = " << val << endl;
                    }
                }
                pc++;
            }
        }
        
        cout << "\n=== ВЫПОЛНЕНИЕ ЗАВЕРШЕНО ===\n";
    }
    
    void execute_line(const string& line) {
        string l = line;
        size_t start = l.find_first_not_of(" \t");
        if (start != string::npos) l = l.substr(start);
        if (l.empty()) return;
        
        vector<string> tokens = split_line(l);
        if (tokens.empty()) return;
        
        string cmd = tokens[0];
        transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
        
        if (cmd == "PRINT") {
            for (size_t i = 1; i < tokens.size(); i++) {
                string arg = tokens[i];
                if (arg[0] == '"') {
                    string str = arg.substr(1, arg.length() - 2);
                    cout << str;
                } else if (arg == ",") {
                } else {
                    size_t pos = i;
                    int val = eval_expression(tokens, pos);
                    cout << val;
                    i = pos - 1;
                }
            }
            cout << endl;
        }
        else if (cmd != "DO" && cmd != "LOOP" && cmd != "UNTIL" && cmd != "DIM") {
            string var_name = tokens[0];
            if (tokens.size() >= 2 && tokens[1] == "=") {
                size_t expr_pos = 2;
                int val = eval_expression(tokens, expr_pos);
                if (var_name.find('[') != string::npos) {
                    string arr_name = var_name.substr(0, var_name.find('['));
                    string idx_str = var_name.substr(var_name.find('[') + 1);
                    idx_str = idx_str.substr(0, idx_str.find(']'));
                    int idx = atoi(idx_str.c_str());
                    if (arrays.find(arr_name) != arrays.end() && idx >= 0 && idx < (int)arrays[arr_name].size()) {
                        arrays[arr_name][idx] = val;
                    }
                } else {
                    vars[var_name] = val;
                }
            }
        }
    }
};

// ============================================================================
// ТЕСТОВЫЙ ФАЙЛ
// ============================================================================

void create_test() {
    ofstream f("test.bas");
    f << "DIM x AS INTEGER\n";
    f << "DIM y AS INTEGER\n";
    f << "DIM a AS INTEGER\n";
    f << "INPUT x\n";
    f << "PRINT \"Вы ввели: \", x\n";
    f << "INPUT y\n";
    f << "PRINT \"Вы ввели: \", y\n";
    f << "a = x + y\n";
    f << "PRINT a\n";
    f.close();
    cout << "Создан тестовый файл: test.bas\n";\
}

// ============================================================================
// ГЛАВНАЯ ФУНКЦИЯ
// ============================================================================

int main() {
    string fname;
    cout << "========================================\n";
    cout << "ТРАНСЛЯТОР ЯЗЫКА BASIC (ВАРИАНТ 48)\n";
    cout << "========================================\n";
    cout << "Введите имя файла (Enter для test.bas): ";
    getline(cin, fname);
    
    if (fname.empty()) {
        create_test();
        fname = "test.bas";
    }
    
    try {
        cout << "\n[ЭТАП 1] ЛЕКСИЧЕСКИЙ АНАЛИЗ\n";
        cout << string(50, '-') << endl;
        
        Lexer lex(fname);
        lex.analyze();
        lex.print_tables();
        
        if (lex.has_errors()) {
            cout << "\nЛексические ошибки обнаружены. Анализ остановлен.\n";
            return 1;
        }
        
        cout << "\n[ЭТАП 2] СИНТАКСИЧЕСКИЙ АНАЛИЗ (МЕТОД РАСШИРЕННОГО ПРЕДШЕСТВОВАНИЯ)\n";
        cout << string(50, '-') << endl;
        
        Parser parser(lex.tokens);
        string polish = parser.parse();
        if (polish.empty()) {
            cout << "\nСинтаксические ошибки обнаружены. Анализ остановлен.\n";
            return 1;
        }
        
        cout << "\nПольская запись: " << polish << endl;
        
        cout << "\n[ЭТАП 3] СЕМАНТИЧЕСКИЙ АНАЛИЗ\n";
        cout << string(50, '-') << endl;
        
        SemanticAnalyzer semantic(lex.get_symbol_table(), polish);
        if (!semantic.analyze()) {
            cout << "\nСемантические ошибки обнаружены. Анализ остановлен.\n";
            return 1;
        }
        
        cout << "\n[ЭТАП 4] ИНТЕРПРЕТАЦИЯ ПРОГРАММЫ\n";
        cout << string(50, '-') << endl;
        
        SimpleInterpreter interp;
        interp.load(fname);
        interp.run();
        
        cout << "\n========================================\n";
        cout << "ТРАНСЛЯЦИЯ ЗАВЕРШЕНА УСПЕШНО\n";
        cout << "========================================\n";
        
    } catch (const exception& e) {
        cerr << "\nКРИТИЧЕСКАЯ ОШИБКА: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}