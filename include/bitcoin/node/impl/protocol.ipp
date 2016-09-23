/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_PROTOCOL_IPP
#define LIBBITCOIN_NODE_PROTOCOL_IPP

#include <cstddef>
#include <utility>
#include <bitcoin/network.hpp>

// This must be excluded due to circularity.
// Instead reply on the include chain from p2p_node to here.
////#include <bitcoin/node/p2p_node.hpp>

namespace libbitcoin {
namespace node {

template <class Protocol>
template <typename... Args>
protocol<Protocol>::protocol(p2p_node& network, network::channel::ptr channel,
    Args&&... args)
  : Protocol(network, channel, std::forward<Args>(args)...),
    node_network_(network)
{
}

template <class Protocol>
size_t protocol<Protocol>::top_height() const
{
    return node_network_.top_height();
}

template <class Protocol>
hash_digest protocol<Protocol>::top_hash() const
{
    return node_network_.top_hash();
}

} // namespace node
} // namespace libbitcoin

#endif
