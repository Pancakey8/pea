#include "parser.hpp"
#include "lexer.hpp"
#include "ast.hpp"
#include <print>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

struct TestCase {
    fs::path pea_path;
    std::optional<fs::path> expected_json;
    std::optional<fs::path> expected_err;
};

std::string read_file(const fs::path& path) {
    std::ifstream in{path};
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool run_test(const TestCase& tc) {
    std::string input = read_file(tc.pea_path);
    Lexer lexer{input};
    Parser parser{lexer};

    auto result = parser.parse();

    if (tc.expected_err) {
        if (result) {
            std::println("FAIL: {}. Expected error, but parse succeeded.", tc.pea_path.filename().string());
            return false;
        }
        std::string expected_err = read_file(*tc.expected_err);
        if (result.error().message != expected_err) {
            std::println("FAIL: {}. Expected error '{}', got '{}'", 
                         tc.pea_path.filename().string(), expected_err, result.error().message);
            return false;
        }
        return true;
    }

    if (!tc.expected_json) {
        std::println("FAIL: {}. No expected output file provided.", tc.pea_path.filename().string());
        return false;
    }

    if (!result) {
        std::println("FAIL: {}. Parse failed: {}:{}: {}", 
                     tc.pea_path.filename().string(), result.error().line, result.error().col, result.error().message);
        return false;
    }

    std::stringstream ss;
    ss << *result;
    std::string actual_json = ss.str();
    std::string expected_json = read_file(*tc.expected_json);

    if (actual_json != expected_json) {
        std::println("FAIL: {}. AST mismatch.", tc.pea_path.filename().string());
        std::println("Expected: {}\nActual:   {}", expected_json, actual_json);
        return false;
    }

    return true;
}

int main() {
    fs::path cases_dir = "test/parser/cases";
    if (!fs::exists(cases_dir)) {
        std::println("Error: Cases directory {} not found.", cases_dir.string());
        return 1;
    }

    std::vector<TestCase> tests;
    for (const auto& entry : fs::directory_iterator(cases_dir)) {
        if (entry.path().extension() == ".pea") {
            fs::path pea_path = entry.path();
            fs::path json_path = pea_path;
            json_path.replace_extension(".json");
            fs::path err_path = pea_path;
            err_path.replace_extension(".err");

            TestCase tc{pea_path, std::nullopt, std::nullopt};
            if (fs::exists(json_path)) tc.expected_json = json_path;
            if (fs::exists(err_path)) tc.expected_err = err_path;
            tests.push_back(tc);
        }
    }

    int passed = 0;
    for (const auto& tc : tests) {
        if (run_test(tc)) {
            std::println("PASS: {}", tc.pea_path.filename().string());
            passed++;
        }
    }

    std::println("\n{} / {} tests passed.", passed, tests.size());
    return (passed == tests.size()) ? 0 : 1;
}
