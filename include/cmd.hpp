#pragma once

#include "common.hpp"
#include <string>
#include <memory>
#include <unordered_map>

namespace cmd {
    class error_undecl_flag_t : public common::error_t {
    public:
        error_undecl_flag_t(std::string_view flag_name)
        : common::error_t(str_red(std::string("undecalred necessary flag: ") + std::string(flag_name))) {}
    };

    class cmd_flag_t {
        std::string name_;
        bool is_necessery_;
        bool is_titeled_;
        
        const size_t max_length_ = 20;
        const std::string_view descripiton_;
        
    protected:
        bool is_setted_ = false;

    public:
        cmd_flag_t(std::string_view name, bool is_necessery, bool is_titeled, std::string_view descripiton)
        : name_(name), is_necessery_(is_necessery), is_titeled_(is_titeled), descripiton_(descripiton) {
            if (name.size() > max_length_)
                throw common::error_t(str_red(std::string("too long name of flag: ") + std::string(name)));
        }

        std::string_view name() const { return name_; }
        bool is_necessery() const noexcept { return is_necessery_; }
        bool is_titeled  () const noexcept { return is_titeled_; }
        bool is_setted   () const noexcept { return is_setted_; }
        
        size_t max_length() const noexcept { return max_length_; }

        std::string_view description() const noexcept { return descripiton_; }

        virtual bool parse(std::string_view flag) = 0;

        virtual ~cmd_flag_t() = default;
    };

    class cmd_program_file_t final : public cmd_flag_t {
        using cmd_flag_t::is_setted_;
        std::string value_;

    public:
        cmd_program_file_t() : cmd_flag_t("program_file", true, false, "file with paracl source code") {}
        const std::string& value() const { return value_; }
        using cmd_flag_t::name;

        bool parse(std::string_view flag) override {
            value_ = flag;
            is_setted_ = true;
            return is_setted_;
        }
    };

    class cmd_is_analyze_only_t final : public cmd_flag_t {
        using cmd_flag_t::is_setted_;
        bool value_ = false;

    public:
        cmd_is_analyze_only_t() : cmd_flag_t("is_analyze_only", false, true, "turn off execution") {}
        bool value() const noexcept { return value_; }
        using cmd_flag_t::name;
        
        bool parse(std::string_view flag) override {
            if (flag == "--analyze_only") {
                value_ = true;
                is_setted_ = true;
            }
            return is_setted_;
        }
    };

    class cmd_is_help_t final : public cmd_flag_t {
        using cmd_flag_t::is_setted_;
        bool value_ = false;

    public:
        cmd_is_help_t() : cmd_flag_t("is_help", false, true, "print info") {}
        bool value() const noexcept { return value_; }
        using cmd_flag_t::name;
        
        bool parse(std::string_view flag) override {
            if (flag == "--help") {
                value_ = true;
                is_setted_ = true;
            }
            return is_setted_;
        }
    };

    class cmd_flags_t {
    protected:
        std::pair<int, int> cnt_flags_;
        std::unordered_map<std::string, std::unique_ptr<cmd_flag_t>> flags_;

    private:
        std::pair<int, int> get_cnt_flags() {
            std::pair<int, int> cnt = {0, 0};
            for (auto& flag_ : flags_) {
                cmd_flag_t& flag = *flag_.second.get();
                
                cnt.second++;
                if (flag.is_necessery())
                    cnt.first++;
            }
            return cnt;
        }

    protected:
        void parse_token(std::string_view cmd_flag) {
            for (auto& flag_ : flags_) {
                cmd_flag_t& flag = *flag_.second.get();
                if (!flag.is_titeled() || flag.is_setted())
                    continue;
            
                if (flag.parse(cmd_flag))
                    return;
            }

            for (auto& flag_ : flags_) {
                cmd_flag_t& flag = *flag_.second.get();
                if (flag.is_titeled() || flag.is_setted())
                    continue;

                if (flag.parse(cmd_flag))
                    return;
            }
        }

    public:
        cmd_flags_t() {
            std::unique_ptr<cmd_program_file_t> program_file = std::make_unique<cmd_program_file_t>();
            flags_.emplace(program_file.get()->name(), std::move(program_file));

            std::unique_ptr<cmd_is_analyze_only_t> is_analyzing = std::make_unique<cmd_is_analyze_only_t>();
            flags_.emplace(is_analyzing.get()->name(), std::move(is_analyzing));

            std::unique_ptr<cmd_is_help_t> is_help = std::make_unique<cmd_is_help_t>();
            flags_.emplace(is_help.get()->name(), std::move(is_help));

            cnt_flags_ = get_cnt_flags();
        }

        void check_valid() {
            for (auto& flag_ : flags_) {
                cmd_flag_t& flag = *flag_.second.get();
                if (flag.is_necessery() && !flag.is_setted())
                    throw error_undecl_flag_t{flag.name()};
            }
        }

        virtual ~cmd_flags_t() = default;
    };

    class cmd_data_t final : public cmd_flags_t {
        using cmd_flags_t::cnt_flags_;
        using cmd_flags_t::flags_;

    public:
        void parse(int argc, char* argv[]) {
            if (argc < cnt_flags_.first)
                throw common::error_t{"Invalid argument: argc = 2, argv[1] = name of file\n"};

            for (int i : view::iota(1, argc))
                cmd_flags_t::parse_token(argv[i]);

            lookup_print_help(std::cout);
            cmd_flags_t::check_valid();
        }

        const std::string& program_file() const {
            cmd_flag_t* flag = flags_.find("program_file")->second.get();
            return static_cast<cmd_program_file_t*>(flag)->value();
        }

        bool is_analyze_only() const noexcept {
            cmd_flag_t* flag = flags_.find("is_analyze_only")->second.get();
            return static_cast<cmd_is_analyze_only_t*>(flag)->value();
        }
        std::ostream& lookup_print_help(std::ostream& os) const {
            cmd_flag_t* flag = flags_.find("is_help")->second.get();
            bool is_help = static_cast<cmd_is_help_t*>(flag)->value();
            if (is_help) {
                for (auto& flag_ : flags_) {
                    os << print_lcyan(flag_.first);

                    int added_scapes = flag_.second->max_length() - flag_.first.size();
                    for (int _ : view::iota(0, added_scapes))
                        os << " ";

                    const cmd_flag_t& flag = *flag_.second.get();
                    os << print_lcyan(flag.description()) << "\n";
                }
            }
            return os;
        }
    };
}