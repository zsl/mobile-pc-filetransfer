#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <regex>
#include <array>
#include <vector>
#include <memory>
#include <utility>
#include <sstream>
#include <thread>

#include "tcpclient.h"

namespace asio = boost::asio;
namespace property_tree = boost::property_tree;

class pc_manager
{
public:
	typedef std::vector<std::pair<std::string, asio::ip::tcp::endpoint>> pcinfo;

public:
	pc_manager(asio::io_service& ioservice, asio::ip::udp::endpoint multicast_endpoint)
		: m_udpsocket(ioservice)
		, m_tcpsocket(ioservice)
		, m_multicast_endpoint(multicast_endpoint)
	{
		m_udpsocket.open(asio::ip::udp::v4());

		property_tree::ptree ptree;
		ptree.put("type", "pcinfo");
		ptree.put("value", "");

		std::stringstream ss;
		property_tree::write_json(ss, ptree);

		std::string json{ ss.str() };

		m_udpsocket.send_to(asio::buffer(json, json.size()), m_multicast_endpoint);

		start_scan_info();
	}

	pcinfo& get_pcinfo() { return m_pcinfo; }

public:
	bool ls(std::string pcname, std::string dirname)
	{
		auto itfind = m_connections.find(pcname);
		if (itfind == m_connections.end())
		{
			std::cerr << "not connect to pc: " << pcname << std::endl;
			return false;
		}

		itfind->second->ls(dirname);
		return true;
	}

	bool connect(std::string pcname)
	{
		if (m_connections.count(pcname))
		{
			return true;
		}

		pcinfo::iterator itfind = std::find_if(m_pcinfo.begin(), m_pcinfo.end(), 
			[&pcname](pcinfo::value_type& v)
			{
				return v.first == pcname;
			}
		);

		if (itfind == m_pcinfo.end())
		{
			// 没有找到指定的PC
			return false;
		}

		std::shared_ptr<tcpclient> client = std::make_shared<tcpclient>(m_udpsocket.get_io_service());
		bool ret = client->connect(itfind->second);
		if (!ret)
		{
			return false;
		}

		m_connections.insert(std::make_pair(pcname, client));
		return true;
	}

private:
	void start_scan_info()
	{
		m_udpsocket.async_receive_from(asio::buffer(m_udpbuf, m_udpbuf.max_size()), m_new_pc_endpoint,
			[this](const boost::system::error_code& e, std::size_t size)
			{
				read_handler(e, size); 
			}
		);
	}

	void read_handler(const boost::system::error_code& e, std::size_t size)
	{
		if (!e)
		{
			std::stringstream ss(std::string(m_udpbuf.begin(), m_udpbuf.begin() + size));

			property_tree::ptree ptree;
			property_tree::read_json(ss, ptree);

			std::string type = ptree.get<std::string>("type", "");
			if (!type.empty())
			{
				if (type == "pcname")
				{
					std::string pcname = ptree.get<std::string>("value");
					m_pcinfo.emplace_back(std::make_pair(pcname, asio::ip::tcp::endpoint(m_new_pc_endpoint.address(), m_new_pc_endpoint.port())));

					std::cout << "new pc: " << pcname << std::endl;

				}
				else if (type == "listdir")
				{
					auto value = ptree.get_child("value");
					for (auto file : value)
					{
						std::cout << file.second.get<std::string>("type")
							<< " : "
							<< file.second.get<std::string>("path")
							<< std::endl;
					}
				}
				else
				{
					std::cerr << "unknow msg:" << ss.str() << std::endl;
				}
			}
			else
			{
				std::cerr << "error msg in " << __FUNCTION__ << ": " << ss.str() << std::endl;
			}
		}
		else
		{
			std::cerr << __FUNCTION__ << ": " << e.value() << ":" << e.message() << "\n";
		}

		start_scan_info();
	}

private:
	asio::ip::udp::socket m_udpsocket;
	std::array<char, 1024> m_udpbuf;
	asio::ip::udp::endpoint m_new_pc_endpoint;
	
	asio::ip::udp::endpoint m_multicast_endpoint;
	
	asio::ip::tcp::socket m_tcpsocket;

	pcinfo m_pcinfo;

	std::map<std::string, std::shared_ptr<tcpclient>> m_connections;
};

class stdinput_handler
{
public:
	stdinput_handler(asio::io_service& ioservice, std::shared_ptr<pc_manager> pcmanager)
	    : m_std_input(ioservice, ::GetStdHandle(STD_INPUT_HANDLE))
		, m_pc_manager(pcmanager)
	{
		start_read();
	}

private:
	void start_read()
	{
		std::cout << ">: ";
		asio::async_read_until(m_std_input, m_streambuf, "\n", 
			boost::bind(&stdinput_handler::read_handler, this
			    , asio::placeholders::error
				, asio::placeholders::bytes_transferred
			)
		);
	}

	void read_handler(const boost::system::error_code& e, std::size_t size)
	{
		if (!e)
		{
			std::istream stream(&m_streambuf);
			
			std::string cmd;
			std::getline(stream, cmd);
			boost::trim(cmd);

			if (cmd == "pcinfo")
			{
				for (auto info : m_pc_manager->get_pcinfo())
				{
					std::cout << info.first << std::endl;
				}
			}

			start_read();
		}
		else
		{
			std::cerr << "error in read_handler\n";
		}
	}

private:
	asio::windows::stream_handle m_std_input;
	asio::streambuf m_streambuf;

	std::shared_ptr<pc_manager> m_pc_manager;
};

static void input_thread(boost::asio::io_service* io_service, std::shared_ptr<pc_manager> pcmanager)
{
	while (!std::cin.eof())
	{
		// std::cout << ">: ";
		std::string cmd;
		std::getline(std::cin, cmd);
		boost::trim(cmd);
		io_service->post([=]{

			if (cmd == "pcinfo")
			{
				for (auto info : pcmanager->get_pcinfo())
				{
					std::cout << info.first << std::endl;
				}

				return;
			}

			std::regex regex_listdir(R"(ls\s([\w-]+)\s*([\w-\\/:]*))");
			std::smatch match_result;
			
			if (std::regex_match(cmd, match_result, regex_listdir))
			{
				if (!pcmanager->ls(match_result[1].str(), match_result[2].str()))
				{
					std::cerr << "cannot find pc: " << match_result[1].str() << std::endl;
				}
				
				return;
			}

			std::regex regex_connect(R"(connect\s([\w-]+)\s*)");

			if (std::regex_match(cmd, match_result, regex_connect))
			{
				if (!pcmanager->connect(match_result[1].str()) )
				{
					std::cerr << "cannot connect to: " << match_result[1].str() << std::endl;
				}
				else
				{
					std::cout << "connect " << match_result[1].str() << " successfully.\n";
				}
			}
			else
			{
				std::cerr << "error cmd in input thread\n";
			}

		});
	}
}

int main()
{
	asio::io_service ioservice;

	auto multicast_address = asio::ip::address::from_string("225.0.0.1");
	asio::ip::udp::endpoint multicast_endpoint(multicast_address, 10000);

	std::shared_ptr<pc_manager> pcmanager(new pc_manager(ioservice, multicast_endpoint));
	// stdinput_handler input_handler(ioservice, pcmanager);

	std::thread thread(&input_thread, &ioservice, pcmanager);

	ioservice.run();
}