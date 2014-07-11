#pragma once

#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/code_converter.hpp>
#include <boost/locale.hpp>

#include <memory>
#include <functional>
#include <string>
#include <array>
#include <regex>
#include <istream>

namespace asio = boost::asio;
namespace property_tree = boost::property_tree;
namespace filesystem = boost::filesystem;
namespace iostreams = boost::iostreams;
namespace locale = boost::locale;

using filesystem::detail::utf8_codecvt_facet;

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

            typedef iostreams::code_converter<std::istream, utf8_codecvt_facet> InCodeConverterDevice;
            iostreams::stream<InCodeConverterDevice> stream(inputstream);

			property_tree::wptree cmdtree;
			property_tree::read_json(stream, cmdtree);

			std::wstring cmd = cmdtree.get<std::wstring>(L"type", L"");

			property_tree::wptree resultptree;
			if (cmd == L"pcinfo")
			{
				resultptree.put(L"type", L"pcname");
				resultptree.put(L"value", locale::conv::to_utf<wchar_t>(asio::ip::host_name(), "utf8"));
			}
			else if (cmd == L"ls")
			{
				bool iserror = false;

				std::wstring strPath = cmdtree.get(L"value", L"");
				property_tree::wptree diritem_tree;
				property_tree::wptree item;
				if (strPath.empty())
				{
					item.put(L"type", L"dir");
					item.put(L"path", L"desktop");
					diritem_tree.push_back(std::make_pair(L"", item));

					item.clear();
					item.put(L"type", L"dir");
					item.put(L"path", L"c:\\");
					diritem_tree.push_back(std::make_pair(L"", item));

					item.clear();
					item.put(L"type", L"dir");
					item.put(L"path", L"d:\\");
					diritem_tree.push_back(std::make_pair(L"", item));

					item.put(L"type", L"dir");
					item.put(L"path", L"e:\\");
					diritem_tree.push_back(std::make_pair(L"", item));
				}
				else
				{
                    filesystem::path path(strPath);

					// 在这里遍历目录
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
					//diritem_tree.push_back(std::make_pair(path, item));

					resultptree.put(L"type", L"listdir");
                    resultptree.put(L"parent", strPath);
					resultptree.push_back(std::make_pair(L"value", diritem_tree));
				}
			}

			std::wstringstream ss;
			property_tree::write_json(ss, resultptree);

			std::wstring utf16Result{ ss.str() };
            std::string result = locale::conv::utf_to_utf<char>(utf16Result);

			if (!result.empty())
			{
				std::cout << "result:\n" << result << std::endl;
				m_response_data = std::move(result);
				
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
