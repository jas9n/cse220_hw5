#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <cstring>

class FileComparisonTest : public ::testing::Test {
public:
    static std::string test_suite;
protected:
    static constexpr int NUM_PLAYERS = 6;
    std::array<std::vector<std::string>, NUM_PLAYERS> expected_lines;
    std::array<std::vector<std::string>, NUM_PLAYERS> actual_lines;

    void SetUp() override {
        // Read all expected files
        for (int i = 0; i < NUM_PLAYERS; ++i) {
            std::string expected_path = "scripts/tests/" + test_suite + "/expected/player" + std::to_string(i) + ".logs";
            std::ifstream expected_file(expected_path);
            std::string line;
            while (std::getline(expected_file, line)) {
                expected_lines[i].push_back(line);
            }
            expected_file.close();
        }

        // Read all actual files
        for (int i = 0; i < NUM_PLAYERS; ++i) {
            std::string actual_path = "logs/player" + std::to_string(i) + ".logs";
            std::ifstream actual_file(actual_path);
            std::string line;
            while (std::getline(actual_file, line)) {
                actual_lines[i].push_back(line);
            }
            actual_file.close();
        }
    }
};

// Initialize static member
std::string FileComparisonTest::test_suite = "test1";

// Test that both files have the same number of lines for each player
TEST_F(FileComparisonTest, SameNumberOfLines) {
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        EXPECT_EQ(expected_lines[i].size(), actual_lines[i].size()) 
            << "Files for player " << i << " have different number of lines in test suite " << test_suite;
    }
}

// Generate individual test cases for each line of each player
TEST_F(FileComparisonTest, CompareLines) {
    for (int player = 0; player < NUM_PLAYERS; ++player) {
        // Use the smaller size to avoid out of bounds access
        size_t num_lines = std::min(expected_lines[player].size(), actual_lines[player].size());
        
        for (size_t i = 0; i < num_lines; ++i) {
            EXPECT_EQ(expected_lines[player][i], actual_lines[player][i]) 
                << "Line " << i + 1 << " differs between files for player " << player 
                << " in test suite " << test_suite;
        }
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Set the test suite name from command line argument
    if (argc > 1) {
        FileComparisonTest::test_suite = argv[1];
    }
    
    return RUN_ALL_TESTS();
}