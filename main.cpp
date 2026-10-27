#include <array>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <iostream>
#include <random>
#include <string>
#ifdef _WIN32
#include <thread>
#endif
#include <utility>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <cerrno>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {
constexpr int kGridSize = 10;
constexpr int kStartingHealth = 3;
constexpr int kRoundSeconds = 3 * 60;
constexpr int kWallCount = 14;
constexpr int kTrapCount = 4;
constexpr int kMaximumMapAttempts = 500;

struct Relic {
    int row;
    int column;
    std::string name;
    int points;
};

enum class GameState { Playing, Won, Lost, Quit };

#ifndef _WIN32
class TerminalMode {
public:
    TerminalMode() {
        if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &original_) == 0) {
            termios raw = original_;
            raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            active_ = tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
        }
    }

    ~TerminalMode() {
        if (active_) tcsetattr(STDIN_FILENO, TCSANOW, &original_);
    }

    bool active() const { return active_; }

private:
    termios original_{};
    bool active_ = false;
};
#endif

class Game {
public:
    Game() : randomEngine_(std::random_device{}()) { initializeGame(); }

    void run() {
        displayTitle();
        std::cout << "\nYou are an explorer sealed inside an ancient ruin. "
                     "Gather every relic, survive the traps, and find the vault exit.\n"
                     "Press W/A/S/D to move. You can quit at any time with Q.\n";

        while (true) {
            playRound();
            displayEndScreen();
            std::cout << "\nWould you like to explore again? (Y/N): ";
            std::string answer;
            if (!std::getline(std::cin, answer) || answer.empty() ||
                std::toupper(static_cast<unsigned char>(answer[0])) != 'Y') {
                break;
            }
            initializeGame();
        }
        std::cout << "\nThe ruins fall quiet. Thanks for playing The Lost Vault.\n";
    }

private:
    std::array<std::string, kGridSize> tiles_{};
    std::vector<Relic> relics_;
    int playerRow_ = 1;
    int playerColumn_ = 1;
    int health_ = kStartingHealth;
    int score_ = 0;
    int moves_ = 0;
    int collected_ = 0;
    GameState state_ = GameState::Playing;
    std::mt19937 randomEngine_;
    std::chrono::steady_clock::time_point deadline_{};

    void initializeGame() {
        tiles_.fill(std::string(kGridSize, '#'));
        for (int row = 1; row < kGridSize - 1; ++row) {
            for (int column = 1; column < kGridSize - 1; ++column) {
                tiles_[row][column] = '.';
            }
        }

        relics_ = {{0, 0, "Sunstone", 10}, {0, 0, "Moon Seal", 15},
                   {0, 0, "Ancient Crown", 25}, {0, 0, "Jade Idol", 15},
                   {0, 0, "Star Compass", 10}};
        generateMap();
        playerRow_ = 1;
        playerColumn_ = 1;
        health_ = kStartingHealth;
        score_ = 0;
        moves_ = 0;
        collected_ = 0;
        state_ = GameState::Playing;
    }

    bool isProtectedCell(int row, int column) const {
        return (row == 1 && column == 1) || (row == 8 && column == 8);
    }

    bool mapIsSolvable() const {
        std::array<std::array<bool, kGridSize>, kGridSize> visited{};
        std::vector<std::pair<int, int>> pending{{1, 1}};
        visited[1][1] = true;
        constexpr std::array<int, 4> rowStep{{-1, 1, 0, 0}};
        constexpr std::array<int, 4> columnStep{{0, 0, -1, 1}};

        for (std::size_t index = 0; index < pending.size(); ++index) {
            const auto [row, column] = pending[index];
            for (std::size_t direction = 0; direction < rowStep.size(); ++direction) {
                const int nextRow = row + rowStep[direction];
                const int nextColumn = column + columnStep[direction];
                if (nextRow >= 1 && nextRow <= 8 && nextColumn >= 1 && nextColumn <= 8 &&
                    tiles_[nextRow][nextColumn] != '#' && !visited[nextRow][nextColumn]) {
                    visited[nextRow][nextColumn] = true;
                    pending.emplace_back(nextRow, nextColumn);
                }
            }
        }

        if (!visited[8][8]) return false;
        for (const Relic& relic : relics_) {
            if (relic.row < 1 || relic.row > 8 || relic.column < 1 || relic.column > 8) continue;
            if (tiles_[relic.row][relic.column] == 'T' && !visited[relic.row][relic.column]) return false;
        }
        return true;
    }

    bool mapIsPlayable() const {
        std::array<int, kGridSize * kGridSize> relicAt{};
        std::array<int, kGridSize * kGridSize> trapAt{};
        relicAt.fill(-1);
        trapAt.fill(-1);
        for (std::size_t i = 0; i < relics_.size(); ++i) {
            relicAt[relics_[i].row * kGridSize + relics_[i].column] = static_cast<int>(i);
        }
        int trapCount = 0;
        for (int row = 1; row <= 8; ++row) {
            for (int column = 1; column <= 8; ++column) {
                if (tiles_[row][column] == '^') {
                    trapAt[row * kGridSize + column] = trapCount++;
                }
            }
        }

        struct SearchState {
            int row;
            int column;
            int relicMask;
            int health;
            int trapMask;
        };
        constexpr int kRelicMaskCount = 1 << 5;
        constexpr int kTrapMaskCount = 1 << kTrapCount;
        constexpr int kStatesPerCell = kRelicMaskCount * (kStartingHealth + 1) * kTrapMaskCount;
        std::vector<bool> visited(kGridSize * kGridSize * kStatesPerCell, false);
        std::vector<SearchState> pending{{1, 1, 0, kStartingHealth, 0}};
        const auto stateIndex = [](int row, int column, int relicMask, int health, int trapMask) {
            const int cell = row * kGridSize + column;
            return (((cell * kRelicMaskCount + relicMask) * (kStartingHealth + 1) + health) *
                    kTrapMaskCount) + trapMask;
        };
        visited[stateIndex(1, 1, 0, kStartingHealth, 0)] = true;
        constexpr std::array<int, 4> rowStep{{-1, 1, 0, 0}};
        constexpr std::array<int, 4> columnStep{{0, 0, -1, 1}};

        for (std::size_t index = 0; index < pending.size(); ++index) {
            const SearchState current = pending[index];
            if (current.row == 8 && current.column == 8 && current.relicMask == kRelicMaskCount - 1) {
                return true;
            }
            for (std::size_t direction = 0; direction < rowStep.size(); ++direction) {
                const int nextRow = current.row + rowStep[direction];
                const int nextColumn = current.column + columnStep[direction];
                if (nextRow < 1 || nextRow > 8 || nextColumn < 1 || nextColumn > 8 ||
                    tiles_[nextRow][nextColumn] == '#') {
                    continue;
                }
                int nextRelicMask = current.relicMask;
                int nextHealth = current.health;
                int nextTrapMask = current.trapMask;
                const int nextCell = nextRow * kGridSize + nextColumn;
                if (relicAt[nextCell] >= 0) nextRelicMask |= 1 << relicAt[nextCell];
                if (trapAt[nextCell] >= 0 &&
                    (nextTrapMask & (1 << trapAt[nextCell])) == 0) {
                    --nextHealth;
                    nextTrapMask |= 1 << trapAt[nextCell];
                    if (nextHealth == 0) continue;
                }

                const int nextIndex = stateIndex(nextRow, nextColumn, nextRelicMask,
                                                 nextHealth, nextTrapMask);
                if (!visited[nextIndex]) {
                    visited[nextIndex] = true;
                    pending.push_back({nextRow, nextColumn, nextRelicMask,
                                       nextHealth, nextTrapMask});
                }
            }
        }
        return false;
    }

    void generateMap() {
        std::vector<std::pair<int, int>> interiorCells;
        for (int row = 1; row <= 8; ++row) {
            for (int column = 1; column <= 8; ++column) {
                interiorCells.emplace_back(row, column);
            }
        }

        for (int attempt = 0; attempt < kMaximumMapAttempts; ++attempt) {
            for (int row = 1; row <= 8; ++row) {
                for (int column = 1; column <= 8; ++column) tiles_[row][column] = '.';
            }
            std::shuffle(interiorCells.begin(), interiorCells.end(), randomEngine_);

            std::size_t placedWalls = 0;
            for (const auto& cell : interiorCells) {
                if (placedWalls == kWallCount) break;
                if (!isProtectedCell(cell.first, cell.second)) {
                    tiles_[cell.first][cell.second] = '#';
                    ++placedWalls;
                }
            }

            if (!mapIsSolvable()) continue;
            std::vector<std::pair<int, int>> openCells;
            for (const auto& cell : interiorCells) {
                if (tiles_[cell.first][cell.second] == '.' && !isProtectedCell(cell.first, cell.second)) {
                    openCells.push_back(cell);
                }
            }
            std::shuffle(openCells.begin(), openCells.end(), randomEngine_);
            if (openCells.size() < relics_.size() + kTrapCount) continue;

            for (std::size_t i = 0; i < relics_.size(); ++i) {
                relics_[i].row = openCells[i].first;
                relics_[i].column = openCells[i].second;
                tiles_[relics_[i].row][relics_[i].column] = 'T';
            }
            for (int i = 0; i < kTrapCount; ++i) {
                const auto& cell = openCells[relics_.size() + static_cast<std::size_t>(i)];
                tiles_[cell.first][cell.second] = '^';
            }
            tiles_[8][8] = 'E';
            if (!mapIsSolvable() || !mapIsPlayable()) continue;
            return;
        }

        // Safe fallback: an open connected interior with the fixed object counts.
        for (int row = 1; row <= 8; ++row) {
            for (int column = 1; column <= 8; ++column) tiles_[row][column] = '.';
        }
        const std::array<std::pair<int, int>, 5> fallbackRelicCells{{
            {1, 8}, {3, 3}, {5, 5}, {7, 2}, {8, 7}}};
        for (std::size_t i = 0; i < relics_.size(); ++i) {
            relics_[i].row = fallbackRelicCells[i].first;
            relics_[i].column = fallbackRelicCells[i].second;
            tiles_[relics_[i].row][relics_[i].column] = 'T';
        }
        tiles_[2][4] = '^';
        tiles_[4][6] = '^';
        tiles_[6][4] = '^';
        tiles_[7][7] = '^';
        tiles_[8][8] = 'E';
    }

    void displayTitle() const {
        std::cout << "====================================\n"
                     "          THE LOST VAULT            \n"
                     "       A 2D Treasure Hunt           \n"
                     "====================================\n";
    }

    void playRound() {
        deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(kRoundSeconds);
        while (state_ == GameState::Playing) {
            if (remainingSeconds() == 0) {
                state_ = GameState::Lost;
                std::cout << "\nTime is up! The vault seals before you can escape.\n";
                break;
            }
            drawGrid();
            displayStatus();
            const std::string input = readTimedLine();
            if (remainingSeconds() == 0) {
                state_ = GameState::Lost;
                std::cout << "\nTime is up! The vault seals before you can escape.\n";
                break;
            }
            if (input.empty()) {
                std::cout << "Please enter a direction (W, A, S, D) or Q.\n\n";
                continue;
            }
            handleInput(static_cast<char>(std::toupper(static_cast<unsigned char>(input[0]))));
        }
    }

    int remainingSeconds() const {
        const auto remaining = deadline_ - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero()) return 0;
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count();
        return static_cast<int>((milliseconds + 999) / 1000);
    }

    static std::string formatTime(int seconds) {
        const int minutes = seconds / 60;
        const int remainder = seconds % 60;
        std::string formatted = (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":";
        if (remainder < 10) formatted += '0';
        formatted += std::to_string(remainder);
        return formatted;
    }

    std::string readTimedLine() {
        std::string input;
        auto lastDisplay = std::chrono::steady_clock::now();
        std::cout << "Move (W/A/S/D), or Q to quit: " << std::flush;
#ifdef _WIN32
        if (_isatty(_fileno(stdin)) == 0) return readPipedLine();
        while (remainingSeconds() > 0) {
            if (_kbhit()) {
                const int key = _getch();
                if (key == '\r' || key == '\n') {
                    std::cout << '\n';
                    return input;
                }
                if (key == 8) {
                    if (!input.empty()) input.pop_back();
                } else if (key >= 32 && key <= 126) {
                    input.push_back(static_cast<char>(key));
                }
                refreshInputDisplay(input);
            }
            if (std::chrono::steady_clock::now() - lastDisplay >= std::chrono::seconds(1)) {
                refreshInputDisplay(input);
                lastDisplay = std::chrono::steady_clock::now();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
        }
#else
        TerminalMode terminal;
        if (!terminal.active()) {
            // Piped input has no interactive wait; retain straightforward line input.
            return readPipedLine();
        }
        while (remainingSeconds() > 0) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(STDIN_FILENO, &readSet);
            timeval timeout{0, 100000};
            const int ready = select(STDIN_FILENO + 1, &readSet, nullptr, nullptr, &timeout);
            if (ready > 0) {
                char key = '\0';
                const ssize_t count = read(STDIN_FILENO, &key, 1);
                if (count == 0) {
                    state_ = GameState::Quit;
                    return {};
                }
                if (count < 0) {
                    if (errno == EINTR) continue;
                    state_ = GameState::Quit;
                    return {};
                }
                if (key == '\r' || key == '\n') {
                    std::cout << '\n';
                    return input;
                }
                if (key == 127 || key == '\b') {
                    if (!input.empty()) input.pop_back();
                } else if (key >= 32 && key <= 126) {
                    input.push_back(key);
                }
                refreshInputDisplay(input);
            } else if (ready < 0 && errno != EINTR) {
                return {};
            }
            if (std::chrono::steady_clock::now() - lastDisplay >= std::chrono::seconds(1)) {
                refreshInputDisplay(input);
                lastDisplay = std::chrono::steady_clock::now();
            }
        }
#endif
        std::cout << '\n';
        return {};
    }

    std::string readPipedLine() {
        std::string input;
        if (!std::getline(std::cin, input)) {
            state_ = GameState::Quit;
            return {};
        }
        return input;
    }

    void refreshInputDisplay(const std::string& input) const {
        std::cout << "\nTime remaining: " << formatTime(remainingSeconds())
                  << " | Move (W/A/S/D), or Q to quit: " << input << std::flush;
    }

    void drawGrid() const {
        std::cout << "\n          THE RUINS\n    ";
        for (int column = 0; column < kGridSize; ++column) std::cout << column << ' ';
        std::cout << '\n';
        for (int row = 0; row < kGridSize; ++row) {
            std::cout << " " << row << "  ";
            for (int column = 0; column < kGridSize; ++column) {
                const char symbol = row == playerRow_ && column == playerColumn_
                                        ? 'P' : tiles_[row][column];
                std::cout << symbol << ' ';
            }
            std::cout << '\n';
        }
        std::cout << "Legend: P You  T Relic  ^ Trap  # Wall  E Exit  . Passage\n";
    }

    void displayStatus() const {
        std::cout << "Health: " << health_ << '/' << kStartingHealth
                  << "   Score: " << score_
                  << "   Relics: " << collected_ << '/' << relics_.size()
                  << "   Moves: " << moves_
                  << "   Time: " << formatTime(remainingSeconds()) << '\n';
    }

    void handleInput(char input) {
        if (input == 'Q') {
            state_ = GameState::Quit;
            std::cout << "You leave the ruins behind.\n";
            return;
        }
        int nextRow = playerRow_;
        int nextColumn = playerColumn_;
        switch (input) {
            case 'W': --nextRow; break;
            case 'A': --nextColumn; break;
            case 'S': ++nextRow; break;
            case 'D': ++nextColumn; break;
            default:
                std::cout << "That command is not recognized. Use W, A, S, D, or Q.\n\n";
                return;
        }
        movePlayer(nextRow, nextColumn);
    }

    void movePlayer(int nextRow, int nextColumn) {
        if (nextRow < 0 || nextRow >= kGridSize || nextColumn < 0 ||
            nextColumn >= kGridSize || tiles_[nextRow][nextColumn] == '#') {
            std::cout << "Stone blocks your way. No move was used.\n\n";
            return;
        }
        playerRow_ = nextRow;
        playerColumn_ = nextColumn;
        ++moves_;

        if (tiles_[playerRow_][playerColumn_] == 'T') collectTreasure();
        else if (tiles_[playerRow_][playerColumn_] == '^') {
            --health_;
            std::cout << "A hidden trap snaps shut! You lose one health point.\n";
            tiles_[playerRow_][playerColumn_] = '.';
            if (health_ == 0) {
                state_ = GameState::Lost;
                std::cout << "Your strength is gone; the ruins claim another explorer.\n\n";
                return;
            }
        } else if (tiles_[playerRow_][playerColumn_] == 'E') {
            const int remaining = static_cast<int>(relics_.size()) - collected_;
            if (remaining == 0) {
                state_ = GameState::Won;
                std::cout << "The vault recognizes its recovered relics. You escape!\n";
            } else {
                std::cout << "The exit remains sealed. " << remaining
                          << " relic" << (remaining == 1 ? "" : "s") << " still missing.\n";
            }
        } else {
            std::cout << "You move deeper into the ruins.\n";
        }
        std::cout << '\n';
    }

    void collectTreasure() {
        for (const Relic& relic : relics_) {
            if (relic.row == playerRow_ && relic.column == playerColumn_) {
                score_ += relic.points;
                ++collected_;
                tiles_[playerRow_][playerColumn_] = '.';
                std::cout << "Relic recovered: " << relic.name << " ("
                          << relic.points << " points)!\n";
                return;
            }
        }
    }

    void displayEndScreen() const {
        std::cout << "\n====================================\n";
        if (state_ == GameState::Won) std::cout << "       THE VAULT IS YOURS!\n";
        else if (state_ == GameState::Lost) std::cout << "          EXPLORER LOST\n";
        else std::cout << "          EXPEDITION ENDED\n";
        std::cout << "====================================\n"
                  << "Final score: " << score_ << '\n'
                  << "Relics collected: " << collected_ << '/' << relics_.size() << '\n'
                  << "Health remaining: " << health_ << '/' << kStartingHealth << '\n'
                  << "Total moves: " << moves_ << '\n';
    }
};
} // namespace

int main() {
    Game game;
    game.run();
    return 0;
}
