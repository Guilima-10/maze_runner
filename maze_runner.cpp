#include <iostream>
#include <fstream>
#include <vector>
#include <stack>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <set>

using Maze = std::vector<std::vector<char>>;

struct Position {
    int row;
    int col;
    bool operator==(const Position& other) const {
        return row == other.row && col == other.col;
    }
    bool operator<(const Position& other) const {
        return row < other.row || (row == other.row && col < other.col);
    }
};

Maze maze;
int num_rows;
int num_cols;
Position exit_pos;        // posição de saída
std::mutex maze_mutex;
std::atomic<bool> exit_found{false};
std::atomic<int> active_threads{0};
std::set<Position> active_positions;


// Função para carregar o labirinto de um arquivo
Position load_maze(const std::string& file_name) {
    std::ifstream file(file_name);
    if (!file) {
        std::cerr << "\nErro ao abrir o arquivo!\n" << std::endl;
        exit(1);
    }

    file >> num_rows >> num_cols;
    maze.resize(num_rows, std::vector<char>(num_cols));
    Position start{-1, -1};

    for (int i = 0; i < num_rows; ++i) {
        for (int j = 0; j < num_cols; ++j) {
            file >> maze[i][j];
            if (maze[i][j] == 'e') {
                start = {i, j};
                maze[i][j] = 'o';
            }
            if (maze[i][j] == 's') {
                exit_pos = {i, j};
            }
        }
    }

    file.close();
    return start;
}


// Função para imprimir o labirinto
void print_maze() {
    system("cls");
    for (const auto& row : maze) {
        for (char cell : row) {
            std::cout << cell << ' ';
        }
        std::cout << '\n';
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Função para verificar se uma posição é válida
bool is_valid_position(int row, int col) {
    return (row >= 0 && row < num_rows && col >= 0 && col < num_cols &&
            (maze[row][col] == 'x' || maze[row][col] == 's' || 
             maze[row][col] == 'o'));
}

// Função para navegar pelo labirinto
void walk(Position current) {
    active_threads++;

    // bloquear o acesso ao lab enquanto atualza
    
    {
        std::lock_guard<std::mutex> lock(maze_mutex);
        active_positions.insert(current);
        if (maze[current.row][current.col] == 'x') {
            maze[current.row][current.col] = 'o';   // marca posicao atual
        }
        print_maze();
    }

    // loop até achar a saida

    while (!exit_found) {
        std::vector<Position> moves = {          // possiveis movimentos
            {current.row + 1, current.col},
            {current.row, current.col + 1},
            {current.row - 1, current.col},
            {current.row, current.col - 1}
        };

        std::vector<Position> valid_moves;
        {
            std::lock_guard<std::mutex> lock(maze_mutex);
            for (auto& move : moves) {
                if (is_valid_position(move.row, move.col)) {
                    valid_moves.push_back(move);
                }
            }
        }

        if (valid_moves.empty()) {   //se nao tem mais para onde ir, termina essa thread
            break;
        }

        // cria threads pros caminhos extras        
        std::vector<std::thread> threads;
        for (size_t i = 1; i < valid_moves.size(); ++i) {
            if (!exit_found) {
                threads.emplace_back(walk, valid_moves[i]);
            }
        }

        {
            std::lock_guard<std::mutex> lock(maze_mutex);
            if (valid_moves[0] == exit_pos) {           // verifica se achou a saida
                exit_found = true;
                maze[current.row][current.col] = '.';
                maze[exit_pos.row][exit_pos.col] = 'o';
                active_positions.clear();
                print_maze();
                std::cout << "\nSaida encontrada!\n" << std::endl;
                break;
            }

            // atualizcao da posicao atual
            active_positions.erase(current);
            if (maze[current.row][current.col] != 'e') {
                maze[current.row][current.col] = '.';
            }
            
            // move p proxima posicao
            current = valid_moves[0];
            active_positions.insert(current);
            maze[current.row][current.col] = 'o';
            print_maze();
        }

        // espera as threads filhas terminarem
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }

        if (exit_found) break;
    }

    {
        // limpeza final antes de termonar a thread
        std::lock_guard<std::mutex> lock(maze_mutex);
        active_positions.erase(current);
        if (maze[current.row][current.col] == 'o') {
            maze[current.row][current.col] = '.';
        }
        if (!exit_found) print_maze();
    }
    
    active_threads--;
}

int main() {
    Position start_pos = load_maze("maze.txt");          // carrega labirinto
    if (start_pos.row == -1 || start_pos.col == -1) {
        std::cerr << "\nPosicao inicial nao encontrada no labirinto.\n" << std::endl;
        return 1;
    }

    std::thread main_thread(walk, start_pos);     // comeca expoloracao pela thread principal  
    main_thread.join();

    while (active_threads > 0) {             // espera todas as threads terminarem
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));    
    }

    if (!exit_found) {                       // erro se nao achar a saida
        std::cout << "\nNao foi possível encontrar a saida.\n" << std::endl;
    }

    return 0;
}
