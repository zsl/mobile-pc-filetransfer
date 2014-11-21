#include "tcp_server.h"
#include "download_session.h"

#include <boost/filesystem.hpp>
#include <boost/locale.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/code_converter.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>

#include <regex>

namespace iostreams = boost::iostreams;
namespace filesystem = boost::filesystem;

using filesystem::detail::utf8_codecvt_facet;

void tcp_connection::handle_read_length(const boost::system::error_code& e, std::size_t bytes_transferred)
{
    if (!e)
    {
        std::istream instream(&m_buf);
        std::string line;
        std::getline(instream, line);
        boost::trim(line);

        std::regex re{ R"(len:(\d+))" };
        std::smatch match;
        if (std::regex_match(line, match, re))
        {
            pointer ptr = shared_from_this();

            std::size_t data_size = boost::lexical_cast<std::size_t>(match[1].str());
            asio::async_read(m_socket, m_buf, asio::transfer_exactly(data_size - m_buf.size()),
                [ptr](const boost::system::error_code& e, std::size_t bytes_transferred)
                {
                    ptr->handle_read_data(e, bytes_transferred);
                }
            );
        }
    }
}

void tcp_connection::handle_read_data(const boost::system::error_code& e, std::size_t bytes_transferred)
{
    if (!e)
    {
        std::istream inputstream(&m_buf);

        typedef iostreams::code_converter<std::istream, utf8_codecvt_facet> InCodeConverterDevice;
        iostreams::stream<InCodeConverterDevice> stream(inputstream);

        property_tree::wptree cmdtree;
        property_tree::read_json(stream, cmdtree);

        std::wstring cmd = cmdtree.get<std::wstring>(L"type", L"");

        // TODO: 把这个if ... else ... 拿出来做成一个 cmd_factory
        if (cmd == L"pcinfo")
        {
            property_tree::wptree resultptree;
            resultptree.put(L"type", L"pcname");
            resultptree.put(L"value", locale::conv::to_utf<wchar_t>(asio::ip::host_name(), "utf8"));
            async_write_response(resultptree);
        }
        else if (cmd == L"ls")
        {
            proc_ls_cmd(cmdtree);
        }
        else if (cmd == L"download")
        {
            proc_download_cmd(cmdtree);
        }

		start_recv();
    }
    else
    {
        std::cerr << "catch err in handle_receive\n";
    }

}

void tcp_connection::proc_ls_cmd(const property_tree::wptree& cmdtree)
{
    property_tree::wptree resultptree;
    bool iserror = false;

    std::wstring strPath = cmdtree.get(L"value", L"");
    property_tree::wptree diritem_tree;
    property_tree::wptree item;

    if (strPath.empty())
    {
        item.put(L"type", L"dir");
        item.put(L"path", L"desktop");
        diritem_tree.push_back(std::make_pair(L"", item));

        std::vector<std::string> rootDirs = GetHardDiskList();
        
        for (size_t i = 0; i < rootDirs.size(); ++i)
        {
            item.clear();
            item.put(L"type", L"dir");
            item.put(L"path", locale::conv::to_utf<wchar_t>(rootDirs[i], "utf8"));
            diritem_tree.push_back(std::make_pair(L"", item));
        }
    }
    else
    {
        filesystem::path path(strPath);

        if (!filesystem::exists(path))
        {
            iserror = true;

            resultptree.put(L"type", L"error");
            resultptree.put(L"value", L"path not exist: " + path.wstring());
        }
        else if (!filesystem::is_directory(path))
        {
            iserror = true;

            resultptree.put(L"type", L"error");
            resultptree.put(L"value", L"path is not directory: " + path.wstring());
        }
        else
        {
            for (auto dirit = filesystem::directory_iterator(path); dirit != filesystem::directory_iterator(); ++dirit)
            {
                item.clear();
                if (filesystem::is_directory(dirit->status()))
                {
                    item.put(L"type", L"dir");
                }
                else if (filesystem::is_regular_file(dirit->status()))
                {
                    item.put(L"type", L"file");
                }
                else continue;

                item.put(L"path", dirit->path().filename().wstring());
                diritem_tree.push_back(std::make_pair(L"", item));
            }
        }
    }

    if (!iserror)
    {
        resultptree.put(L"type", L"listdir");
        resultptree.put(L"parent", strPath);
        resultptree.push_back(std::make_pair(L"value", diritem_tree));
    }

    async_write_response(resultptree);
}

void tcp_connection::proc_download_cmd(const property_tree::wptree& cmdtree)
{
    std::wstring filePath = cmdtree.get(L"value", L"");

    assert(!filePath.empty());

    if (!filesystem::exists(filePath))
    {
        property_tree::wptree resultptree;
        resultptree.put(L"type", L"error");
        resultptree.put(L"value", L"path not exists: " + filePath);
        async_write_response(resultptree);
    }
    else if (!filesystem::is_regular_file(filePath))
    {
        property_tree::wptree resultptree;
        resultptree.put(L"type", L"error");
        resultptree.put(L"value", L"path is not file: " + filePath);
        async_write_response(resultptree);
    }
    else
    {
        auto downsession = std::make_shared<download_session>(shared_from_this(), filePath);
        downsession->start();
    }
}