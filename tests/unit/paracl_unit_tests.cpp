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

TEST(Paracl_shuffle, end_to_end) {
    std::string file{__FILE__};
    std::filesystem::path dir = file.substr(0, file.rfind("/"));
    std::filesystem::path tests_dir = dir / "../end_to_end";

    std::filesystem::path answers_get_path = tests_dir / "answers_get";
    std::filesystem::path answers_src_path = tests_dir / "answers_src";

    std::vector<std::string> answers_get_str = get_sorted_files(answers_get_path);
    std::vector<std::string> answers_src_str = get_sorted_files(answers_src_path);
    const int count_tests = std::min(answers_get_str.size(), answers_src_str.size());

    std::string ans_string;
    for (int i = 0; i < count_tests; ++i) {
        std::ifstream answer_get_file(answers_get_str[i]);
        std::vector<std::string> ans_get;
        while (std::getline(answer_get_file, ans_string))
            ans_get.push_back(ans_string);
        answer_get_file.close();

        std::ifstream answer_src_file(answers_src_str[i]);
        std::vector<std::string> ans_src;
        while (std::getline(answer_src_file, ans_string))
            ans_src.push_back(ans_string);
        answer_src_file.close();

        EXPECT_EQ(ans_get.size(), ans_src.size()) << "test: " << i + 1;
        for (int j = 0, end = ans_get.size(); j < end; ++j)
            EXPECT_EQ(ans_get[j], ans_src[j]) << "test: " << i + 1 << " string: " << j + 1;
    }
}