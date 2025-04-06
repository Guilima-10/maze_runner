#include <iostream>
#include <fstream>
#include <vector>
#include <stack>
#include <thread>
#include <chrono>
#include <mutex>
#include <algorithm>


using Maze = std::vector<std::vector<char>>;


struct Position {
    int row;
    int col;
};

Maze maze;
int num_rows;
int num_cols;
Position exit_pos;
std::mutex maze_mutex;
bool exit_found = false;

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

bool is_valid_position(int row, int col) {
    return (row >= 0 && row < num_rows && col >= 0 && col < num_cols &&
            (maze[row][col] == 'x' || maze[row][col] == 's' || maze[row][col] == 'o'));
}

int calculate_distance(Position p) {
    return std::abs(p.row - exit_pos.row) + std::abs(p.col - exit_pos.col);
}

void explore(Position current) {
    while (true) {
        if (exit_found) return;

        std::vector<Position> moves = {
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

        if (valid_moves.empty()) {
            std::lock_guard<std::mutex> lock(maze_mutex);
            if (maze[current.row][current.col] == 'o') {
                maze[current.row][current.col] = '.';
                print_maze();
            }
            return;
        }

        std::sort(valid_moves.begin(), valid_moves.end(), [](Position a, Position b) {
            return calculate_distance(a) < calculate_distance(b);
        });

        for (size_t i = 1; i < valid_moves.size(); ++i) {
            std::thread(explore, valid_moves[i]).detach();
        }


        {
            std::lock_guard<std::mutex> lock(maze_mutex);
        
            if (maze[valid_moves[0].row][valid_moves[0].col] == 'o') {
                // Encontrou outro caminho ('o')
                // Verifica se ainda há outro caminho possível além desse que leva ao encontro
                bool has_other_path = false;
                for (size_t i = 1; i < valid_moves.size(); ++i) {
                    if (maze[valid_moves[i].row][valid_moves[i].col] == 'x' || maze[valid_moves[i].row][valid_moves[i].col] == 's') {
                        has_other_path = true;
                        break;
                    }
                }
            
                if (has_other_path) {
                    // Marca a colisão como passada e segue pelo próximo caminho
                    maze[current.row][current.col] = '.';
                    print_maze();
                    // Continua com a thread pelo novo caminho
                    current = valid_moves[1];  // vá para o segundo melhor caminho
                    continue;
                } else {
                    // Nenhum outro caminho viável, então encerra essa thread
                    maze[current.row][current.col] = '.';
                    maze[valid_moves[0].row][valid_moves[0].col] = '.';
                    print_maze();
                    return;
                }
            }
            
        
            if (maze[current.row][current.col] != 'e') {
                maze[current.row][current.col] = '.';
            }
            maze[valid_moves[0].row][valid_moves[0].col] = 'o';
            print_maze();
        }
        



        if (valid_moves[0].row == exit_pos.row && valid_moves[0].col == exit_pos.col) {
            std::lock_guard<std::mutex> lock(maze_mutex);
            exit_found = true;
            print_maze();
            std::cout << "\nSaida encontrada!\n" << std::endl;
            return;
        }

        current = valid_moves[0];
    }
}


int main() {
    Position start_pos = load_maze("maze.txt");
    if (start_pos.row == -1 || start_pos.col == -1) {
        std::cerr << "\nPosicao inicial nao encontrada no labirinto.\n" << std::endl;
        return 1;
    }

    std::thread main_thread(explore, start_pos);
    main_thread.join();

    if (!exit_found) {
        std::cout << "\nNao foi possível encontrar a saida.\n" << std::endl;
    }

    return 0;
}
