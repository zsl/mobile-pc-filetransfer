#pragma once

#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

#include <memory>
#include <functional>
#include <string>
#include <array>
#include <regex>
#include <istream>

namespace asio = boost::asio;
namespace property_tree = boost::property_tree;
namespace filesystem = boost::filesystem;

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

private:
	void handle_read_length(const boost::system::error_code& e, std::size_t bytes_transferred)
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

	void handle_read_data(const boost::system::error_code& e, std::size_t bytes_transferred)
	{
		if (!e)
		{
			std::istream inputstream(&m_buf);

			property_tree::ptree cmdtree;
			property_tree::read_json(inputstream, cmdtree);

			std::string cmd = cmdtree.get<std::string>("type", "");

			property_tree::ptree resultptree;
			if (cmd == "pcinfo")
			{
				resultptree.put("type", "pcname");
				resultptree.put("value", asio::ip::host_name());
			}
			else if (cmd == "ls")
			{
				bool iserror = false;

				std::string path = cmdtree.get("value", "");
				property_tree::ptree diritem_tree;
				property_tree::ptree item;
				if (path.empty())
				{
					item.put("type", "dir");
					item.put("path", "desktop");
					diritem_tree.push_back(std::make_pair("", item));

					item.clear();
					item.put("type", "dir");
					item.put("path", "c:\\");
					diritem_tree.push_back(std::make_pair("", item));

					item.clear();
					item.put("type", "dir");
					item.put("path", "d:\\");
					diritem_tree.push_back(std::make_pair("", item));

					item.put("type", "dir");
					item.put("path", "e:\\");
					diritem_tree.push_back(std::make_pair("", item));
				}
				else
				{
					// 在这里遍历目录
					if (!filesystem::exists(path))
					{
						iserror = true;

						resultptree.put("type", "error");
						resultptree.put("value", "path not exist: " + path);
					}
					else if (!filesystem::is_directory(path))
					{
						iserror = true;

						resultptree.put("type", "error");
						resultptree.put("value", "path is not directory: " + path);
					}
					else
					{
						for (auto dirit = filesystem::directory_iterator(path); dirit != filesystem::directory_iterator(); ++dirit)
						{
							item.clear();
							if (filesystem::is_directory(dirit->status()))
							{
								item.put("type", "dir");
							}
							else if (filesystem::is_regular_file(dirit->status()))
							{
								item.put("type", "file");
							}
							else continue;

							item.put("path", dirit->path().filename());
							diritem_tree.push_back(std::make_pair("", item));
						}
					}
				}

				if (!iserror)
				{
					//diritem_tree.push_back(std::make_pair(path, item));

					resultptree.put("type", "listdir");
					resultptree.push_back(std::make_pair("value", diritem_tree));
				}
			}

			std::stringstream ss;
			property_tree::write_json(ss, resultptree);

			std::string result{ ss.str() };

			if (!result.empty())
			{
				std::cout << "result:\n" << result << std::endl;
				m_response_data = std::move(ss.str());
				
				m_response_len.append("len:");
				m_response_len += boost::lexical_cast<std::string>(m_response_data.size());
				m_response_len.append("\r\n");

				std::vector<asio::const_buffer> buffers{ 
					asio::buffer(m_response_len, m_response_len.size()), 
					asio::buffer(m_response_data, m_response_data.size())
				};

				pointer ptr = shared_from_this();
				asio::async_write(m_socket, buffers, 
						[ptr](const boost::system::error_code& e, std::size_t bytes_transferred)
						{
							ptr->handle_write(e, bytes_transferred);
						}
				);
			}

		}
		else
		{
			std::cerr << "catch err in handle_receive\n";
		}
	
	}

	void handle_write(const boost::system::error_code& e, std::size_t bytes_transferred)
	{
		if (!e)
		{
			m_response_data.clear();
			m_response_len.clear();
		}
		start_recv();
	}

private:
	tcp_connection(asio::io_service& ioservice)
		: m_socket(ioservice)
	{
	}

private:
	asio::ip::tcp::socket m_socket;
	asio::streambuf m_buf;

	std::string m_response_len;
	std::string m_response_data;
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
					new_connection->start_recv();
				}

				start_acept();
			}
		);
	}
private:
	asio::ip::tcp::acceptor m_acceptor;
};
