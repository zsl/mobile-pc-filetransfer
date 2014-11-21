#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/locale.hpp>

#include <string>
#include <tuple>

namespace property_tree = boost::property_tree;
namespace locale = boost::locale;

inline std::tuple<std::string, std::string> gen_cmd_from_ptree(const property_tree::wptree& resultptree)
{
    std::wstringstream ss;
    property_tree::write_json(ss, resultptree);

    std::wstring utf16Result{ ss.str() };
    std::string result = locale::conv::utf_to_utf<char>(utf16Result);

    if (!result.empty())
    {
        std::cout << "result:\n" << result << std::endl;

        std::string response_len("len:");
        response_len += boost::lexical_cast<std::string>(result.size());
        response_len.append("\r\n");

        return std::make_tuple(std::move(response_len), std::move(result));
    }
    else
    {
        return std::make_tuple("", "");
    }
}

inline std::locale get_chinese_locale()
{
    // 系统默认的locale不一定是中文的
    std::locale chslocale;
    try
    {
        chslocale = std::locale("chs");
    }
    catch (std::exception &e)
    {
        chslocale = std::locale("");
    }

    return chslocale;
}

class auto_chinese_locale
{
public:
    auto_chinese_locale(){
        oldlocale = std::wcout.imbue(get_chinese_locale());
    }

    ~auto_chinese_locale(){
        std::wcout.imbue(oldlocale);
    }
private:
    std::locale oldlocale;
};
