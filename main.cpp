#include <array>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int kGridSize = 10;
constexpr int kStartingHealth = 3;

struct Relic {
    int row;
    int column;
    std::string name;
    int points;
};

enum class GameState { Playing, Won, Lost, Quit };

class Game {
public:
    Game() { initializeGame(); }

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

    void initializeGame() {
        // The outer stone wall and interior passages form the fixed, solvable map.
        tiles_ = {{"##########", "#...#..T.#", "#.#.#.##.#", "#...T....#",
                   "###.#.##.#", "#T..^....#", "#.#.###..#", "#...T...E#",
                   "#..^....T#", "##########"}};
        relics_ = {{3, 4, "Sunstone", 10}, {5, 1, "Moon Seal", 15},
                   {7, 4, "Ancient Crown", 25}, {8, 8, "Jade Idol", 15},
                   {1, 7, "Star Compass", 10}};
        playerRow_ = 1;
        playerColumn_ = 1;
        health_ = kStartingHealth;
        score_ = 0;
        moves_ = 0;
        collected_ = 0;
        state_ = GameState::Playing;
    }

    void displayTitle() const {
        std::cout << "====================================\n"
                     "          THE LOST VAULT            \n"
                     "       A 2D Treasure Hunt           \n"
                     "====================================\n";
    }

    void playRound() {
        while (state_ == GameState::Playing) {
            drawGrid();
            displayStatus();
            std::cout << "Move (W/A/S/D), or Q to quit: ";
            std::string input;
            if (!std::getline(std::cin, input)) {
                state_ = GameState::Quit;
                break;
            }
            if (input.empty()) {
                std::cout << "Please enter a direction (W, A, S, D) or Q.\n\n";
                continue;
            }
            handleInput(static_cast<char>(std::toupper(static_cast<unsigned char>(input[0]))));
        }
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
                  << "   Moves: " << moves_ << '\n';
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
