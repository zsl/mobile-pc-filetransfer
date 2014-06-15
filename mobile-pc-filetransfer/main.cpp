#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>

#include <array>
#include <string>
#include <sstream>
#include <iostream>

#include "tcp_server.h"

namespace asio = boost::asio;
namespace property_tree = boost::property_tree;
namespace filesystem = boost::filesystem;

class udp_server
{
public:
	udp_server(asio::io_service& ioservice, asio::ip::udp::endpoint &endpoint)
		: m_socket(ioservice, endpoint)
	{
		// auto multicast_address = endpoint.address();
		auto multicast_address = asio::ip::address::from_string("225.0.0.1");
		asio::ip::multicast::join_group join_group_option(multicast_address);
		
		m_socket.set_option(join_group_option);

		start_receive();
	}

private:
	void start_receive()
	{
		m_socket.async_receive_from(asio::buffer(m_buf), m_endpoint, 
			boost::bind(&udp_server::handle_receive, this
			    , asio::placeholders::error
				, asio::placeholders::bytes_transferred
			) 
		);
	}
	void handle_receive(const boost::system::error_code err, std::size_t bytes_transferred)
	{
		if (!err)
		{
			std::stringstream ss(std::string(m_buf.begin(), m_buf.begin() + bytes_transferred));

			property_tree::ptree cmdtree;
			property_tree::read_json(ss, cmdtree);

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

			ss.str("");
			property_tree::write_json(ss, resultptree);

			std::string result{ ss.str() };

			if (!result.empty())
			{
				std::cout << "result:\n" << result << std::endl;
				m_socket.send_to(asio::buffer(result), m_endpoint);
			}

			start_receive();
		}
		else
		{
			if (err == asio::error::message_size)
			{
				std::cerr << "error: message size\n";
			}
			else
			{
				std::cerr << "catch err in handle_receive:" << err << std::endl;
			}

			start_receive();
		}
	}

private:
	asio::ip::udp::socket m_socket;

	std::array<char, 512> m_buf;
	asio::ip::udp::endpoint m_endpoint;
};

int main()
{
	asio::io_service ioservice;

	auto multicast_address = asio::ip::address::from_string("225.0.0.1");

	// udp_server udpserver(ioservice, asio::ip::udp::endpoint(multicast_address, 10000));
	udp_server udpserver(ioservice, asio::ip::udp::endpoint(asio::ip::udp::v4(), 10000));
	tcp_server tcpserver(ioservice, 10000);

	ioservice.run();
}