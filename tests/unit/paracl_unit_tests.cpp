#include <gtest/gtest.h>

#include <fstream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <fstream>

std::vector<std::string> get_sorted_files(std::filesystem::path path) {
    std::vector<std::string> files;

    for (const auto& entry : std::filesystem::directory_iterator(path))
        files.push_back(entry.path().string());

    std::sort(files.begin(), files.end());
    return files;
}

TEST(Matrix_shuffle, end_to_end) {
    std::string file{__FILE__};
    std::filesystem::path dir = file.substr(0, file.rfind("/"));
    std::filesystem::path tests_dir = dir / "../end_to_end";

    std::filesystem::path answers_get_path = tests_dir / "answers_get";
    std::filesystem::path answers_src_path = tests_dir / "answers_src";

    std::vector<std::string> answers_get_str = get_sorted_files(answers_get_path);
    std::vector<std::string> answers_src_str = get_sorted_files(answers_src_path);
    const int count_tests = std::min(answers_get_str.size(), answers_src_str.size());

    for (int i = 0, count; i < count_tests; ++i) {
        std::ifstream answer_get_file(answers_get_str[i]);
        std::vector<int> ans_get;
        while (answer_get_file >> count)
            ans_get.push_back(count);
        answer_get_file.close();

        std::ifstream answer_src_file(answers_src_str[i]);
        std::vector<int> ans_src;
        while (answer_src_file >> count)
            ans_src.push_back(count);
        answer_src_file.close();

        EXPECT_EQ(ans_get.size(), ans_src.size());
        for (int j = 0, end = ans_get.size(); j < end; ++j)
            EXPECT_EQ(ans_get[j], ans_src[j]);
    }
}