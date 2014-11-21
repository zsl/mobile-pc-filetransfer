#pragma once

#include "HardDisk.h"
#include "msg.h"
#include "util.h"

#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <memory>
#include <functional>
#include <string>
#include <array>
#include <istream>
#include <queue>

namespace asio = boost::asio;
namespace property_tree = boost::property_tree;


class tcp_connection
	: public std::enable_shared_from_this<tcp_connection>
{
public:

	typedef std::shared_ptr<tcp_connection> pointer;

	static pointer create(asio::io_service& ioservice)
	{
		return pointer(new tcp_connection(ioservice));
	}

	asio::ip::tcp::socket& socket(){ return m_socket; }

    asio::io_service& get_io_service() { return m_io_service;  }

	void start_recv()
	{
		pointer ptr = shared_from_this();
		m_buf.consume(m_buf.size());
		asio::async_read_until(m_socket, m_buf, "\r\n",
			[ptr](const boost::system::error_code& e, std::size_t bytes_transferred)
			{
				ptr->handle_read_length(e, bytes_transferred);
			}
		);
	}

    void write_msg(const message& msg)
    {
        m_io_service.post(boost::bind(&tcp_connection::do_write, shared_from_this(), msg));
    }

    // 注意，只有在asyn_write的消息处理函数中才能调用这个方法
    void post_write_msg()
    {
        m_write_msgs.pop_front();

        if (!m_write_msgs.empty())
        {
            asio::async_write(m_socket, m_write_msgs.front().buffers, m_write_msgs.front().write_handler);
        }
    }

private:
    void handle_read_length(const boost::system::error_code& e, std::size_t bytes_transferred);
    void handle_read_data(const boost::system::error_code& e, std::size_t bytes_transferred);

private:
	tcp_connection(asio::io_service& ioservice)
        : m_io_service(ioservice)
        , m_socket(ioservice)
	{
	}

private:
    void proc_ls_cmd(const property_tree::wptree& cmdtree);
    void proc_download_cmd(const property_tree::wptree& cmdtree);

    void do_write(message msg)
    {
        bool write_in_progress = !m_write_msgs.empty();
        m_write_msgs.push_back(msg);

        if (!write_in_progress)
        {
            asio::async_write(m_socket, m_write_msgs.front().buffers, m_write_msgs.front().write_handler);
        }
    }

	void handle_write(const boost::system::error_code& e, std::size_t bytes_transferred)
	{
		if (!e)
		{
			m_response_data.clear();
			m_response_len.clear();
		}
        post_write_msg();
	}

    void async_write_response(const property_tree::wptree& resultptree)
    {
        std::tie(m_response_len, m_response_data) = gen_cmd_from_ptree(resultptree);

        if (!m_response_len.empty())
        {
            std::vector<asio::const_buffer> buffers{ 
                asio::buffer(m_response_len, m_response_len.size()), 
                asio::buffer(m_response_data, m_response_data.size())
            };

            message msg{std::move(buffers), boost::bind(&tcp_connection::handle_write, shared_from_this()
                                                           , asio::placeholders::error
                                                           , asio::placeholders::bytes_transferred
                                                       )};
            

            write_msg(msg);
        }
    }


private:
    asio::io_service& m_io_service;
	asio::ip::tcp::socket m_socket;
	asio::streambuf m_buf;

	std::string m_response_len;
	std::string m_response_data;

    std::deque<message> m_write_msgs;
};

class tcp_server
{
public:
	tcp_server(asio::io_service& ioservice, unsigned short port)
		: m_acceptor(ioservice)
	{
		m_acceptor.open(asio::ip::tcp::v4());
		m_acceptor.set_option(asio::socket_base::reuse_address(true));
		m_acceptor.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));
		m_acceptor.listen();
		start_acept();
	}

private:
	void start_acept()
	{
		tcp_connection::pointer	new_connection = tcp_connection::create(m_acceptor.get_io_service());

		m_acceptor.async_accept(new_connection->socket(), 
			[=](const boost::system::error_code& error)
			{
				if (!error)
				{
                    std::cout << "accept a connect.\n";
					new_connection->start_recv();
				}
                else
                {
                    std::cerr << "accept error: " << error << std::endl;
                }

				start_acept();
			}
		);
	}
private:
	asio::ip::tcp::acceptor m_acceptor;
};
