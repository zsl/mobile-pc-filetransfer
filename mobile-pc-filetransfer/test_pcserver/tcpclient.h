#pragma once

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <regex>

namespace asio = boost::asio;
namespace property_tree = boost::property_tree;

class tcpclient
{
public:
	tcpclient(asio::io_service& ioservice)
		: m_socket(ioservice, asio::ip::tcp::v4())
		, m_bconnect(false)
	{}

	void ls(const std::string& dirname)
	{
		property_tree::ptree ptree;
		ptree.put("type", "ls");
		ptree.put("value", dirname);

		std::stringstream ss;
		property_tree::write_json(ss, ptree);

		m_msgdata = std::move(ss.str());

		m_msghead.clear();
		m_msghead.append("len:")
				 .append(boost::lexical_cast<std::string>(m_msgdata.size()))
				 .append("\r\n");

		std::vector<asio::const_buffer> buffers{
			asio::buffer(m_msghead, m_msghead.size()),
			asio::buffer(m_msgdata, m_msgdata.size())
		};

		std::cout << "send to server:\n"
				  << m_msghead << m_msgdata << std::endl;

		asio::async_write(m_socket, buffers,
			[this](const boost::system::error_code &ec, std::size_t bytes_transferred)
			{
				if (!ec)
				{
					asio::async_read_until(m_socket, m_recvbuf, "\r\n",
						boost::bind(&tcpclient::handle_read_head, this, 
						    asio::placeholders::error, asio::placeholders::bytes_transferred
						)
					);
				}
			}
		);
	}

	bool connect(const asio::ip::tcp::endpoint &endpoint)
	{
		boost::system::error_code ec;
		m_socket.connect(endpoint, ec);

		if (ec)
		{
			std::cerr << ec << std::endl;
			return false;
		}

		return true;
	}

private:
	void handle_read_head(const boost::system::error_code &ec, std::size_t bytes_transferred)
	{
		if (!ec)
		{
			std::string line;
			std::istream inputstream(&m_recvbuf);
			std::getline(inputstream, line);

			std::regex re{ R"(len:(\d+)\s*)" };
			std::smatch match;
			if (std::regex_match(line, match, re))
			{
				std::size_t data_size = boost::lexical_cast<std::size_t>(match[1].str());
				asio::async_read(m_socket, m_recvbuf, asio::transfer_exactly(data_size - m_recvbuf.size()),
					[this](const boost::system::error_code& e, std::size_t bytes_transferred)
					{
						handle_read_data(e, bytes_transferred);
					}
				);
			}
		}

	}

	void handle_read_data(const boost::system::error_code &ec, std::size_t bytes_transferred)
	{
		if (!ec)
		{
			std::istream inputstream(&m_recvbuf);

			property_tree::ptree ptree;
			property_tree::read_json(inputstream, ptree);

			std::string cmd = ptree.get<std::string>("type", "");

			if (cmd == "listdir")
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
			else if (cmd == "error")
			{
				std::string value = ptree.get("value", "");
				std::cout << "serve error: " << value << std::endl;
			}
		}
		else
		{
			std::cerr << __FUNCTION__ << " error.\n";
		}
	}

private:
	asio::ip::tcp::socket m_socket;
	bool m_bconnect;
	asio::streambuf m_recvbuf;

	std::string m_msghead;
	std::string m_msgdata;
};