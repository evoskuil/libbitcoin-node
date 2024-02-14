/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_CONNECT_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_CONNECT_HPP

#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down blocks in the the candidate header chain for validation.
class BCN_API chaser_connect
  : public chaser, protected network::tracker<chaser_connect>
{
public:
    typedef std::unique_ptr<chaser_connect> uptr;

    chaser_connect(full_node& node) NOEXCEPT;

private:
    void handle_start() NOEXCEPT;
    void handle_checked() NOEXCEPT;
    void handle_event(const code& ec, chase event_, link value) NOEXCEPT;
    void do_handle_event(const code& ec, chase event_, link value) NOEXCEPT;
};

} // namespace node
} // namespace libbitcoin

#endif