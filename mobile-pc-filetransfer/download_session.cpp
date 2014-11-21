#include "download_session.h"
#include "util.h"
#include "constdef.h"
#include "msg.h"

#include <Windows.h>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace filesystem = boost::filesystem;

void download_session::start()
{
    m_file_handle = ::CreateFileW(m_filepath.c_str(), GENERIC_READ
                                                    , FILE_SHARE_READ
                                                    , nullptr
                                                    , OPEN_EXISTING
                                                    , FILE_ATTRIBUTE_NORMAL|FILE_FLAG_NO_BUFFERING|FILE_FLAG_OVERLAPPED
                                                    , 0);


    if (INVALID_HANDLE_VALUE == m_file_handle)
    {
        property_tree::wptree errorInfo = gen_err_info(L"open file failed");

        std::tie(m_response_len, m_response_data) = gen_cmd_from_ptree(errorInfo);

        std::vector<asio::const_buffer> buffers { asio::buffer(m_response_len, m_response_len.size())
                                          , asio::buffer(m_response_data, m_response_data.size())
        };

        message msg {std::move(buffers), boost::bind(&download_session::handle_finish, shared_from_this()
                                                     , asio::placeholders::error
                                                     , asio::placeholders::bytes_transferred)
        };

        m_con->write_msg(msg);
    }
    else
    {
        m_asio_file.assign(m_file_handle);
        do_readfile();
    }
}

void download_session::do_readfile()
{
    m_file_data.resize(4 * 1024 * 1024);
    asio::async_read(m_asio_file, asio::buffer(m_file_data, m_file_data.size())
                                , boost::bind(&download_session::handle_readfile, shared_from_this()
                                               , asio::placeholders::error
                                               , asio::placeholders::bytes_transferred));
}

void download_session::handle_readfile(const boost::system::error_code& e, size_t bytes_transferred)
{
    if (!e || e.value() == asio::error::eof)
    {
        bool bfinish = e.value() == asio::error::eof;
        property_tree::wptree resultptree = gen_download_info(m_file_pos, bytes_transferred, bfinish);

        std::tie(m_response_len, m_response_data) = gen_cmd_from_ptree(resultptree);

        std::vector<asio::const_buffer> buffers{
            asio::buffer(m_response_len, m_response_len.size()),
            asio::buffer(m_response_data, m_response_data.size()),
            asio::buffer(m_file_data, bytes_transferred),
        };

        message msg{ buffers, boost::bind(bfinish ? &download_session::handle_finish : &download_session::handle_download
                                              , shared_from_this()
                                              , asio::placeholders::error
                                              , asio::placeholders::bytes_transferred) 
        };

        m_con->write_msg(msg);
    }
    else
    {
        // TODO: 读文件出错，向手机汇报错误，并结束传输
        std::cout << "error in " << __FUNCTION__ << ": " << e << std::endl;
    }
}

void download_session::handle_download(const boost::system::error_code& e, size_t bytes_transferred)
{
    m_con->post_write_msg();

    if (!e)
    {
        do_readfile();
    }
    else
    {
        // TODO: 发送文件出错了，服务端要记录这个消息
    }
}

void download_session::handle_finish(const boost::system::error_code& e, size_t bytes_transferred)
{
    m_con->post_write_msg();

    if (INVALID_HANDLE_VALUE != m_file_handle)
    {
        ::CloseHandle(m_file_handle);
    }
}

property_tree::wptree download_session::gen_download_info(size_t pos, size_t readsize, bool bfinish)
{
    property_tree::wptree fileinfo;
    fileinfo.put(L"path", m_filepath);
    fileinfo.put(L"pos", pos);
    fileinfo.put(L"readsize", readsize);
    fileinfo.put(L"finish", bfinish ? L"true" : L"false");

    property_tree::wptree resultptree;
    resultptree.put(L"type", L"download");
    resultptree.push_back(std::make_pair(L"value", fileinfo));

    return std::move(resultptree);
}

property_tree::wptree download_session::gen_err_info(const std::wstring errinfo)
{
    property_tree::wptree errorInfo;
    errorInfo.put(L"type", L"download");
    errorInfo.put(L"value", m_filepath);
    errorInfo.put(L"detail", L"open file failed");

    return std::move(errorInfo);
}
