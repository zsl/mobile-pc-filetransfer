#pragma once
#include "tcp_server.h"

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>

#include <string>
#include <vector>
#include <memory>
#include <cassert>

namespace asio = boost::asio;

class tcp_connection;

// 处理手机下载电脑文件的逻辑
class download_session : public boost::noncopyable
                       , public std::enable_shared_from_this<download_session>
{
public:
    download_session(std::shared_ptr<tcp_connection> con, std::wstring& filepath)
        : m_con(con)
        , m_filepath(filepath)
        , m_file_handle(INVALID_HANDLE_VALUE)
        , m_asio_file(con->get_io_service())
        , m_file_pos(0)
    {
    }

    void start();

private:
    property_tree::wptree gen_err_info(const std::wstring errinfo);
    property_tree::wptree gen_download_info(size_t pos, size_t readsize, bool bfinish);
    void do_readfile();
    void handle_readfile(const boost::system::error_code& e, size_t bytes_transferred);
    void handle_download(const boost::system::error_code& e, size_t bytes_transferred);
    void handle_finish(const boost::system::error_code& e, size_t bytes_transferred);
    
private:
    std::shared_ptr<tcp_connection> m_con;
    std::wstring m_filepath;

    HANDLE m_file_handle;
    asio::windows::stream_handle m_asio_file;
    size_t m_file_pos;

	std::string m_response_len;
	std::string m_response_data;
    std::vector<char> m_file_data;
};