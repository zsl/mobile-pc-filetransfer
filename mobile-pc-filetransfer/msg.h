#pragma once

#include <boost/asio.hpp>

#include <vector>
#include <functional>

class message
{
public:
    std::vector<boost::asio::const_buffer> buffers;
    std::function<void(const boost::system::error_code&, std::size_t)> write_handler;
};